#include <group.h>
#include <kernel.h>
#include <startup.h>
#include <stdio.h>
#include <cp15.h>
// struct group_mgmt* global_group_mgmt __attribute__((section(".text")));
static void group_init_globals(void)
{
    // __asm volatile (
    //     ".global global_group_mgmt             ""\n\t"
    //     ".type global_group_mgmt, STT_OBJECT   ""\n\t"
    //     "global_group_mgmt:                    ""\n\t"
    //     "  .word 0                             ""\n\t"
    // );

    global_group_mgmt = (struct group_mgmt*)bsp_alloc_phys(sizeof(struct group_mgmt));
    printf("MGMT: %lx\n", global_group_mgmt);
}

static lvaddr_t get_got_base(void)
{
    lvaddr_t origin;
    __asm__ volatile (
        "mrc p15, 0, %[origin_got], c13, c0, 4\n\t"
        :[origin_got] "=r" (origin)
    );
    return origin;
}

static lvaddr_t set_got_base_lazy(lvaddr_t got_base) {
    lvaddr_t origin = get_got_base();
    __asm__ volatile (
        "mcr p15, 0, %[got_base], c13, c0, 4\n\t"
        ::[got_base] "r" (got_base)
    );
    return origin;
}

struct group* get_group(coreid_t coreid)
{
    return &global_group_mgmt->groups[coreid];
}

static struct group* set_cur_group_by_coreid(coreid_t coreid, struct group* g)
{
    // here should be more state maintain
    // 比如说当前group都包含了哪些core
    return global_group_mgmt->cur_group[coreid] = g;
}

struct group* get_cur_group_by_coreid(coreid_t coreid)
{
    return global_group_mgmt->cur_group[coreid];
}

struct group* get_cur_group(void)
{
    coreid_t id = get_core_id();
    struct group** target = &global_group_mgmt->lazy_load_target_group[id];
    if (*target && !global_group_mgmt->can_update[id]) {
        printf("cannot update yet\n");
    }
    if (global_group_mgmt->can_update[id] && *target) {
        while (1);
        struct group* to = *target;
        *target = NULL;
        printf("updating\n");
        set_cur_group_by_coreid(id, to);
        while(1);
    }
    return get_cur_group_by_coreid(id);
}

void set_cur_group_lazy(struct group* g)
{
    printf("???????*target: %x coreid: %d ????, got: %x\n", g->group_id, get_core_id(), g->got_base);
    global_group_mgmt->lazy_load_target_group[get_core_id()] = g;
    global_group_mgmt->can_update[get_core_id()] = false;
    // XXX this should be moved to arch specified code
    set_got_base_lazy(g->got_base);

    // prepare per core state in target group
    g->per_core_state[get_core_id()].dcb_current = GROUP_PER_CORE_DCB_CURRENT;
    g->per_core_state[get_core_id()].kcb_current = NULL;
}

static void group_init_common(void)
{
    coreid_t coreid = get_core_id();
    struct group* cur = set_cur_group_by_coreid(coreid, get_group(coreid));
    cur->enabled = true;
    cur->got_base = set_got_base_lazy(get_got_base());
    cur->group_id = coreid;
    if (arch_core_is_bsp()) {
        cur->lock = (volatile int*)bsp_alloc_phys_aligned(4, 4);
    } else {
        cur->lock = (volatile int*)app_alloc_phys_aligned(4, 4);
    }
    *cur->lock = 0;
    cur->per_core_state[coreid].enabled = true;

    printf("MGMT: %lx, got_base: %x\n", global_group_mgmt, cur->got_base);
}

void group_bsp_init(void)
{
    group_init_globals();
    group_init_common();
}

void group_app_init(void)
{
    group_init_common();
}
