#include "chams.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define ENTITY_INDEX_OFFSET 68

Interface_t *g_pModelRender = NULL;
Interface_t *g_pRenderView = NULL;
DrawModelExecuteFn Original_DrawModelExecute = NULL;
void *g_pHiddenMaterial = NULL;
void *g_pVisibleMaterial = NULL;

static atomic_bool g_hook_installed = false;
static atomic_bool g_first_draw_logged = false;
static _Thread_local bool g_inside_chams_hook = false;

static const char HIDDEN_VMT[] =
    "\"VertexLitGeneric\"\n"
    "{\n"
    "\t\"$basetexture\" \"vgui/white_additive\"\n"
    "\t\"$model\"       \"1\"\n"
    "\t\"$nofog\"       \"1\"\n"
    "\t\"$nocull\"      \"1\"\n"
    "\t\"$ignorez\"     \"1\"\n"
    "\t\"$decal\"       \"0\"\n"
    "}\n";

static const char VISIBLE_VMT[] =
    "\"VertexLitGeneric\"\n"
    "{\n"
    "\t\"$basetexture\" \"vgui/white_additive\"\n"
    "\t\"$model\"       \"1\"\n"
    "\t\"$nofog\"       \"1\"\n"
    "\t\"$nocull\"      \"1\"\n"
    "\t\"$ignorez\"     \"0\"\n"
    "\t\"$decal\"       \"0\"\n"
    "}\n";

// 0 idle, 1 validating input, 2 preparing state, 3 hidden pass,  4 visible pass, 5 restoring state, 10 original-only pass.
_Atomic int g_chams_stage = 0;

static bool get_page_protection(const void *address, int *protection);
static size_t get_readable_prefix(const void *address, size_t maximum);

static bool join_path(char *destination, size_t capacity,
                      const char *base, const char *suffix) {
    const size_t base_length = strlen(base);
    const size_t suffix_length = strlen(suffix);
    if (base_length + suffix_length + 1 > capacity) return false;
    memcpy(destination, base, base_length);
    memcpy(destination + base_length, suffix, suffix_length + 1);
    return true;
}

static bool write_all(int descriptor, const char *data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        const ssize_t written = write(descriptor, data + offset,
                                      size - offset);
        if (written > 0) {
            offset += (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

static bool write_atomic_file(const char *path, const char *contents,
                              size_t size) {
    char temporary[PATH_MAX];
    if (!join_path(temporary, sizeof temporary, path, ".tmp.XXXXXX"))
        return false;

    const int descriptor = mkstemp(temporary);
    if (descriptor < 0) return false;

    bool ok = fchmod(descriptor, 0644) == 0 &&
              write_all(descriptor, contents, size) &&
              fsync(descriptor) == 0;
    if (close(descriptor) != 0) ok = false;
    if (ok && rename(temporary, path) != 0) ok = false;
    if (!ok) unlink(temporary);
    return ok;
}

static bool provision_vmt_pair(void) {
    char executable[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", executable,
                                    sizeof executable - 1);
    if (length <= 0 || (size_t)length >= sizeof executable - 1) {
        EngineMsg("[LANNISTER] Chams: cannot resolve /proc/self/exe: %s\n",
                  strerror(errno));
        return false;
    }
    executable[length] = '\0';

    char *separator = strrchr(executable, '/');
    if (!separator || separator == executable) {
        EngineMsg("[LANNISTER] Chams: invalid executable path: %s\n",
                  executable);
        return false;
    }
    *separator = '\0';

    char directory[PATH_MAX];
    char hidden_path[PATH_MAX];
    char visible_path[PATH_MAX];
    if (!join_path(directory, sizeof directory, executable,
                   "/cstrike/materials/" MATERIAL_DIRECTORY_NAME) ||
        !join_path(hidden_path, sizeof hidden_path, directory,
                   "/invisible.vmt") ||
        !join_path(visible_path, sizeof visible_path, directory,
                   "/visible.vmt")) {
        EngineMsg("[LANNISTER] Chams: material path exceeds PATH_MAX\n");
        return false;
    }

    if (mkdir(directory, 0755) != 0 && errno != EEXIST) {
        EngineMsg("[LANNISTER] Chams: mkdir failed for %s: %s\n",
                  directory, strerror(errno));
        return false;
    }

    struct stat status;
    if (stat(directory, &status) != 0 || !S_ISDIR(status.st_mode)) {
        EngineMsg("[LANNISTER] Chams: material path is not a directory: %s\n",
                  directory);
        return false;
    }

    if (!write_atomic_file(hidden_path, HIDDEN_VMT,
                           sizeof HIDDEN_VMT - 1) ||
        !write_atomic_file(visible_path, VISIBLE_VMT,
                           sizeof VISIBLE_VMT - 1)) {
        EngineMsg("[LANNISTER] Chams: atomic VMT write failed in %s: %s\n",
                  directory, strerror(errno));
        return false;
    }

    const int directory_fd = open(directory,
                                  O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory_fd >= 0) {
        (void)fsync(directory_fd);
        close(directory_fd);
    }

    EngineMsg("[LANNISTER] Chams: VMT pair synchronized at %s\n",
              directory);
    return true;
}

static bool safe_copy_from_self(void *destination, const void *source,
                                size_t size) {
    if (!destination || !source || size == 0) return false;

    struct iovec local = {.iov_base = destination, .iov_len = size};
    struct iovec remote = {.iov_base = (void *)source, .iov_len = size};
    const ssize_t copied = process_vm_readv(getpid(), &local, 1,
                                            &remote, 1, 0);
    if (copied == (ssize_t)size) return true;

    // some sandboxes block process_vm_readv even for the current process
    // f]all back only after /proc/self/maps proves the complete range readable
    if (get_readable_prefix(source, size) != size) return false;
    memcpy(destination, source, size);
    return true;
}

static bool is_code_pointer(const void *address) {
    Dl_info info;
    int protection = 0;
    return address && dladdr(address, &info) != 0 && info.dli_fbase != NULL &&
           get_page_protection(address, &protection) &&
           (protection & PROT_EXEC) != 0;
}

static bool has_method(const Interface_t *object, size_t index) {
    return object && object->vtable && is_code_pointer(object->vtable[index]);
}

static bool material_has_method(const void *material, size_t index) {
    if (!material) return false;
    void **vtable = *(void ***)material;
    return vtable && is_code_pointer(vtable[index]);
}

static bool get_page_protection(const void *address, int *protection) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) return false;

    const uintptr_t target = (uintptr_t)address;
    unsigned long start = 0;
    unsigned long end = 0;
    char permissions[5] = {0};
    char line[512];
    bool found = false;

    while (fgets(line, sizeof line, maps)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, permissions) != 3)
            continue;
        if (target < (uintptr_t)start || target >= (uintptr_t)end)
            continue;

        int value = 0;
        if (permissions[0] == 'r') value |= PROT_READ;
        if (permissions[1] == 'w') value |= PROT_WRITE;
        if (permissions[2] == 'x') value |= PROT_EXEC;
        *protection = value;
        found = true;
        break;
    }

    fclose(maps);
    return found;
}

static size_t get_readable_prefix(const void *address, size_t maximum) {
    if (!address || maximum == 0) return 0;

    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) return 0;

    const uintptr_t target = (uintptr_t)address;
    unsigned long start = 0;
    unsigned long end = 0;
    char permissions[5] = {0};
    char line[512];
    size_t result = 0;

    while (fgets(line, sizeof line, maps)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, permissions) != 3)
            continue;
        if (target < (uintptr_t)start || target >= (uintptr_t)end)
            continue;
        if (permissions[0] != 'r') break;

        const uintptr_t available = (uintptr_t)end - target;
        result = available < maximum ? (size_t)available : maximum;
        break;
    }

    fclose(maps);
    return result;
}

static void add_reference(void *material) {
    void **vtable = *(void ***)material;
    IncrementRefCountFn add_ref =
        (IncrementRefCountFn)vtable[IDX_INCREMENT_REF_COUNT];
    add_ref(material);
}

static void release_reference(void *material) {
    if (!material || !material_has_method(material, IDX_DECREMENT_REF_COUNT))
        return;

    void **vtable = *(void ***)material;
    DecrementRefCountFn release =
        (DecrementRefCountFn)vtable[IDX_DECREMENT_REF_COUNT];
    release(material);
}

static void set_render_state(const float color[3], float blend) {
    SetColorModulationFn set_color =
        (SetColorModulationFn)g_pRenderView->vtable[IDX_RENDER_SET_COLOR];
    SetBlendFn set_blend =
        (SetBlendFn)g_pRenderView->vtable[IDX_RENDER_SET_BLEND];
    set_color(g_pRenderView, color);
    set_blend(g_pRenderView, blend);
}

static void force_material(Interface_t *model_render, void *material) {
    ForcedMaterialOverrideFn override =
        (ForcedMaterialOverrideFn)model_render->vtable[IDX_FORCED_MAT_OVERRIDE];
    override(model_render, material, 0, 0);
}

static bool is_player_entity(const void *info, int *entity_index_out) {
    if (!info) return false;

    int entity_index = 0;
    if (!safe_copy_from_self(&entity_index,
                             (const unsigned char *)info + ENTITY_INDEX_OFFSET,
                             sizeof entity_index))
        return false;
    if (entity_index < 1 || entity_index > 64) return false;
    if (entity_index_out) *entity_index_out = entity_index;
    return true;
}

static void *find_usable_material(Interface_t *material_system,
                                  const char *name) {
    FindMaterialFn find_material =
        (FindMaterialFn)material_system->vtable[IDX_FIND_MATERIAL];
    void *material = find_material(material_system, name,
                                   "Model textures", true, NULL);
    if (!material ||
        !material_has_method(material, IDX_IS_ERROR_MATERIAL) ||
        !material_has_method(material, IDX_INCREMENT_REF_COUNT))
        return NULL;

    void **vtable = *(void ***)material;
    IsErrorMaterialFn is_error =
        (IsErrorMaterialFn)vtable[IDX_IS_ERROR_MATERIAL];
    return is_error(material) ? NULL : material;
}

static bool install_hook(void) {
    if (atomic_load_explicit(&g_hook_installed, memory_order_acquire))
        return true;

    void **vtable = g_pModelRender->vtable;
    void *entry = __atomic_load_n(&vtable[IDX_DRAW_MODEL_EXECUTE],
                                  __ATOMIC_ACQUIRE);
    if (!is_code_pointer(entry)) {
        EngineMsg("[LANNISTER] Chams: DrawModelExecute pointer is invalid\n");
        return false;
    }
    if (entry == (void *)Hooked_DrawModelExecute) {
        atomic_store_explicit(&g_hook_installed, true, memory_order_release);
        return true;
    }

    int original_protection = 0;
    if (!get_page_protection(&vtable[IDX_DRAW_MODEL_EXECUTE],
                             &original_protection)) {
        EngineMsg("[LANNISTER] Chams: could not resolve vtable page protection\n");
        return false;
    }

    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) return false;
    void *page = (void *)((uintptr_t)&vtable[IDX_DRAW_MODEL_EXECUTE] &
                          ~((uintptr_t)page_size - 1));

    if (mprotect(page, (size_t)page_size,
                 original_protection | PROT_WRITE) != 0) {
        EngineMsg("[LANNISTER] Chams: mprotect write failed: %s\n",
                  strerror(errno));
        return false;
    }

    Original_DrawModelExecute = (DrawModelExecuteFn)entry;
    __atomic_store_n(&vtable[IDX_DRAW_MODEL_EXECUTE],
                     (void *)Hooked_DrawModelExecute, __ATOMIC_RELEASE);

    if (mprotect(page, (size_t)page_size, original_protection) != 0) {
        __atomic_store_n(&vtable[IDX_DRAW_MODEL_EXECUTE], entry,
                         __ATOMIC_RELEASE);
        Original_DrawModelExecute = NULL;
        (void)mprotect(page, (size_t)page_size, original_protection);
        EngineMsg("[LANNISTER] Chams: page protection restoration failed: %s\n",
                  strerror(errno));
        return false;
    }

    atomic_store_explicit(&g_hook_installed, true, memory_order_release);
    EngineMsg("[LANNISTER] Chams: Linux vtable hook installed at index %d\n",
              IDX_DRAW_MODEL_EXECUTE);
    return true;
}

void Hooked_DrawModelExecute(Interface_t *thisptr, const void *state,
                             const void *pInfo, void *pCustomBoneToWorld) {
    DrawModelExecuteFn original = Original_DrawModelExecute;
    if (!original) return;

    if (g_inside_chams_hook) {
        original(thisptr, state, pInfo, pCustomBoneToWorld);
        return;
    }
    g_inside_chams_hook = true;

    atomic_store_explicit(&g_chams_stage, 1, memory_order_relaxed);
    int entity_index = 0;
    if (!g_pHiddenMaterial || !g_pVisibleMaterial ||
        !is_player_entity(pInfo, &entity_index)) {
        atomic_store_explicit(&g_chams_stage, 10, memory_order_relaxed);
        original(thisptr, state, pInfo, pCustomBoneToWorld);
        atomic_store_explicit(&g_chams_stage, 0, memory_order_relaxed);
        g_inside_chams_hook = false;
        return;
    }

    atomic_store_explicit(&g_chams_stage, 2, memory_order_relaxed);

    const float hidden_color[3] = {0.94f, 0.27f, 0.27f};
    atomic_store_explicit(&g_chams_stage, 3, memory_order_relaxed);
    set_render_state(hidden_color, 1.0f);
    force_material(thisptr, g_pHiddenMaterial);
    original(thisptr, state, pInfo, pCustomBoneToWorld);

    const float visible_color[3] = {0.13f, 0.77f, 0.37f};
    atomic_store_explicit(&g_chams_stage, 4, memory_order_relaxed);
    set_render_state(visible_color, 1.0f);
    force_material(thisptr, g_pVisibleMaterial);
    original(thisptr, state, pInfo, pCustomBoneToWorld);

    atomic_store_explicit(&g_chams_stage, 5, memory_order_relaxed);
    force_material(thisptr, NULL);
    const float neutral_color[3] = {1.0f, 1.0f, 1.0f};
    set_render_state(neutral_color, 1.0f);

    if (!atomic_exchange_explicit(&g_first_draw_logged, true,
                                  memory_order_relaxed))
        EngineMsg("[LANNISTER] Chams: first dual pass drawn for slot %d\n",
                  entity_index);
    atomic_store_explicit(&g_chams_stage, 0, memory_order_relaxed);
    g_inside_chams_hook = false;
}

bool chams_initialize(Interface_t *model_render,
                      Interface_t *material_system,
                      Interface_t *render_view) {
    if (!model_render || !material_system || !render_view) {
        EngineMsg("[LANNISTER] Chams: required interface is missing\n");
        return false;
    }

    if (!has_method(model_render, IDX_DRAW_MODEL_EXECUTE) ||
        !has_method(model_render, IDX_FORCED_MAT_OVERRIDE) ||
        !has_method(material_system, IDX_FIND_MATERIAL) ||
        !has_method(render_view, IDX_RENDER_SET_BLEND) ||
        !has_method(render_view, IDX_RENDER_SET_COLOR)) {
        EngineMsg("[LANNISTER] Chams: required Linux vtable method is invalid\n");
        return false;
    }

    if (!provision_vmt_pair()) {
        EngineMsg("[LANNISTER] Chams: VMT provisioning failed\n");
        return false;
    }

    void *hidden = find_usable_material(material_system,
                                        HIDDEN_MATERIAL_NAME);
    void *visible = find_usable_material(material_system,
                                         VISIBLE_MATERIAL_NAME);
    if (!hidden || !visible || hidden == visible) {
        EngineMsg("[LANNISTER] Chams: expected distinct VMT materials are "
                  "unavailable: %s (%p), %s (%p)\n",
                  HIDDEN_MATERIAL_NAME, hidden,
                  VISIBLE_MATERIAL_NAME, visible);
        return false;
    }

    g_pModelRender = model_render;
    g_pRenderView = render_view;
    g_pHiddenMaterial = hidden;
    g_pVisibleMaterial = visible;
    add_reference(hidden);
    add_reference(visible);

    if (!install_hook()) {
        release_reference(hidden);
        release_reference(visible);
        g_pHiddenMaterial = NULL;
        g_pVisibleMaterial = NULL;
        return false;
    }

    EngineMsg("[LANNISTER] Chams: build=linux-vmt-autoprovision-4, "
              "hidden=%s (%p), "
              "visible=%s (%p), dual-pass enabled\n",
              HIDDEN_MATERIAL_NAME, hidden,
              VISIBLE_MATERIAL_NAME, visible);
    return true;
}
