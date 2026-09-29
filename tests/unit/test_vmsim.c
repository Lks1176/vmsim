/*
 * test_vmsim.c - Pruebas unitarias sin dependencias externas.
 * Cada test_* verifica un módulo por separado; CHECK acumula los fallos para
 * que un error no oculte a los siguientes.
 */
#include <stdio.h>
#include <string.h>

#include "app/command_parser.h"
#include "core/address.h"
#include "core/config.h"
#include "core/numparse.h"
#include "memory/page_table.h"
#include "memory/region_table.h"
#include "memory/swap.h"
#include "policy/replacement.h"
#include "vm/virtual_memory.h"

static int failures = 0;
static int checks = 0;
#define PARSER_TEST_FILE "build/parser_test_input.tmp"

#define CHECK(cond)                                                              \
    do {                                                                         \
        checks++;                                                                \
        if (!(cond)) {                                                           \
            failures++;                                                          \
            fprintf(stderr, "FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                        \
    } while (0)

static void test_numparse(void)
{
    uint32_t v = 0;
    CHECK(parse_u32("4096", &v) && v == 4096u);
    CHECK(parse_u32("0x1000", &v) && v == 4096u);
    CHECK(!parse_u32("-1", &v));
    CHECK(!parse_u32("12abc", &v));
    CHECK(!parse_u32("", &v));
    CHECK(!parse_u32("4294967296", &v)); /* 2^32 no cabe en uint32 */
    CHECK(parse_u32("4294967295", &v) && v == 4294967295u);
}

static void test_layout(void)
{
    AddressLayout l;
    CHECK(layout_init(&l, 4096) == VM_OK);
    CHECK(l.offset_bits == 12 && l.pt1_bits == 10 && l.pt2_bits == 10);
    const uint32_t va = (5u << 22) | (7u << 12) | 0x123u;
    CHECK(layout_pt1_index(&l, va) == 5u);
    CHECK(layout_pt2_index(&l, va) == 7u);
    CHECK(layout_offset(&l, va) == 0x123u);
    CHECK(layout_vpn_to_va(&l, layout_vpn(&l, va)) == (va & ~0xFFFu));
    CHECK(layout_init(&l, 8192) == VM_OK && l.pt1_bits + l.pt2_bits + l.offset_bits == 32);
    CHECK(layout_init(&l, 3000) == VM_ERR_INVALID_ARG);
    CHECK(layout_init(&l, 128) == VM_ERR_INVALID_ARG);
}

static void test_page_table(void)
{
    AddressLayout l;
    layout_init(&l, 4096);
    PageTable *pt = pt_create(&l);
    CHECK(pt != NULL);
    const uint32_t va = 0x00403000u;
    CHECK(pt_lookup(pt, va) == NULL); /* nivel 2 aún no existe */
    bool created = false;
    PageTableEntry *pte = pt_get_or_create(pt, va, &created);
    CHECK(pte != NULL && created && !pte->valid);
    pte->valid = 1;
    CHECK(pt_get_or_create(pt, va, &created) == pte && !created);
    CHECK(pt_lookup(pt, va) == pte);
    CHECK(pt_release_range(pt, va, 1) == 1); /* tabla vacía -> se libera */
    CHECK(pt_lookup(pt, va) == NULL);
    pt_destroy(pt);
}

static void test_regions(void)
{
    AddressLayout l;
    layout_init(&l, 4096);
    RegionTable *rt = rt_create(&l);
    Region a, b;
    CHECK(rt_reserve(rt, 8192, &a) == VM_OK && a.start == 0 && a.num_pages == 2);
    CHECK(rt_reserve(rt, 1, &b) == VM_OK && b.start == 8192 && b.num_pages == 1);
    CHECK(rt_contains(rt, 0) && rt_contains(rt, 8191) && rt_contains(rt, 8192));
    CHECK(!rt_contains(rt, 12288));
    CHECK(rt_reserve(rt, 0, &a) == VM_ERR_INVALID_ARG);
    CHECK(rt_release(rt, 4096, &a) == VM_ERR_BAD_FREE);
    CHECK(rt_release(rt, 0, &a) == VM_OK && !rt_contains(rt, 0) && rt_contains(rt, 8192));
    CHECK(rt_reserve(rt, 0xFFFFFFFFu, &a) == VM_ERR_OUT_OF_VIRTUAL_SPACE);
    rt_destroy(rt);
}

static void test_swap(void)
{
    Swap *swap = swap_create(8);
    uint32_t s0, s1;
    uint8_t out[8];
    const uint8_t in[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(swap_alloc_slot(swap, &s0) == VM_OK && swap_alloc_slot(swap, &s1) == VM_OK);
    CHECK(s0 != s1);
    CHECK(swap_write(swap, s0, in) == VM_OK && swap_read(swap, s0, out) == VM_OK);
    CHECK(memcmp(in, out, 8) == 0);
    CHECK(swap_free_slot(swap, s0) == VM_OK && swap_free_slot(swap, s0) == VM_ERR_INTERNAL);
    uint32_t reused;
    CHECK(swap_alloc_slot(swap, &reused) == VM_OK && reused == s0); /* se recicla */
    swap_destroy(swap);
}

static void test_fifo_order(void)
{
    ReplacementPolicy *p = replacement_create(POLICY_FIFO, 4);
    uint32_t v;
    CHECK(p != NULL && !p->select_victim(p, &v)); /* vacía: sin víctima */
    p->on_load(p, 2);
    p->on_load(p, 0);
    p->on_load(p, 3);
    p->on_access(p, 2); /* FIFO ignora los accesos */
    CHECK(p->select_victim(p, &v) && v == 2);
    p->on_release(p, 0); /* sacar del MEDIO no rompe la cola */
    p->on_release(p, 2);
    CHECK(p->select_victim(p, &v) && v == 3);
    p->on_load(p, 2); /* reingresa al final */
    p->on_release(p, 3);
    CHECK(p->select_victim(p, &v) && v == 2);
    p->destroy(p);
}

static FILE *parser_input(const char *text)
{
    FILE *f = fopen(PARSER_TEST_FILE, "w");
    if (f == NULL) {
        return NULL;
    }
    fputs(text, f);
    fclose(f);
    return fopen(PARSER_TEST_FILE, "r");
}

static void test_parser(void)
{
    const char *text = "alloc 8192 write 0x10 42\n# comentario\nread 7 FREE 0\nwrite 1 300\n";
    FILE *f = parser_input(text);
    CommandParser p;
    Command c;
    parser_init(&p, f);
    CHECK(parser_next(&p, &c) == PARSE_OK && c.type == CMD_ALLOC && c.bytes == 8192);
    CHECK(parser_next(&p, &c) == PARSE_OK && c.type == CMD_WRITE && c.address == 16 && c.value == 42);
    CHECK(parser_next(&p, &c) == PARSE_OK && c.type == CMD_READ && c.address == 7 && c.line == 3);
    CHECK(parser_next(&p, &c) == PARSE_OK && c.type == CMD_FREE);
    CHECK(parser_next(&p, &c) == PARSE_ERROR); /* valor 300 > 255 */
    fclose(f);
    remove(PARSER_TEST_FILE);

    f = parser_input("read\n");
    parser_init(&p, f);
    CHECK(parser_next(&p, &c) == PARSE_ERROR); /* falta argumento */
    fclose(f);
    remove(PARSER_TEST_FILE);

    f = parser_input("jump 3");
    parser_init(&p, f);
    CHECK(parser_next(&p, &c) == PARSE_ERROR); /* comando desconocido */
    fclose(f);
    remove(PARSER_TEST_FILE);
}

static void test_config(void)
{
    Config c;
    config_set_defaults(&c);
    CHECK(config_validate(&c) == NULL && config_num_frames(&c) == 64u);
    c.phys_mem_bytes = 128u * 1024u;
    CHECK(config_validate(&c) != NULL); /* menos de 256 KB */
    c.phys_mem_bytes = 256u * 1024u;
    c.page_size = 3000;
    CHECK(config_validate(&c) != NULL);
    char *argv_ok[] = {"vmsim", "-m", "512", "-s", "8192", "-q", "in.txt"};
    CHECK(config_from_args(&c, 7, argv_ok) == CLI_RUN && c.phys_mem_bytes == 512u * 1024u &&
          c.page_size == 8192u && c.verbosity == VERBOSITY_QUIET);
    char *argv_lru[] = {"vmsim", "-p", "lru"};
    CHECK(config_from_args(&c, 3, argv_lru) == CLI_ERROR);
}

/* Prueba de integración: los datos sobreviven a expulsiones (swap) y se cuenta bien. */
static void test_vm_eviction_integrity(void)
{
    Config cfg;
    config_set_defaults(&cfg);
    cfg.verbosity = VERBOSITY_QUIET;
    VirtualMemory *vm = NULL;
    CHECK(vm_create(&vm, &cfg) == VM_OK);

    uint32_t base;
    CHECK(vm_alloc(vm, 100u * 4096u, &base) == VM_OK && base == 0);
    for (uint32_t i = 0; i < 100; i++) {
        CHECK(vm_write(vm, i * 4096u + 5u, (uint8_t)(i + 1u)) == VM_OK);
    }
    CHECK(vm_stats(vm)->page_faults == 100 && vm_stats(vm)->replacements == 36);
    for (uint32_t i = 0; i < 100; i++) {
        uint8_t v = 0;
        CHECK(vm_read(vm, i * 4096u + 5u, &v) == VM_OK && v == (uint8_t)(i + 1u));
    }
    CHECK(vm_stats(vm)->swap_outs > 0 && vm_stats(vm)->swap_ins > 0);

    uint8_t v = 99;
    CHECK(vm_read(vm, 100u * 4096u, &v) == VM_ERR_SEGFAULT); /* fuera de la reserva */
    CHECK(vm_stats(vm)->total_accesses == 200); /* el segfault no cuenta */

    CHECK(vm_free(vm, base) == VM_OK);
    CHECK(vm_read(vm, 0, &v) == VM_ERR_SEGFAULT);
    CHECK(vm_free(vm, base) == VM_ERR_BAD_FREE);
    CHECK(vm_stats(vm)->l2_tables_freed == 1);

    /* Tras liberar, la memoria física vuelve a estar disponible sin reemplazos. */
    const uint64_t repl_before = vm_stats(vm)->replacements;
    uint32_t again;
    CHECK(vm_alloc(vm, 64u * 4096u, &again) == VM_OK);
    for (uint32_t i = 0; i < 64; i++) {
        CHECK(vm_write(vm, again + i * 4096u, 1) == VM_OK);
    }
    CHECK(vm_stats(vm)->replacements == repl_before);
    vm_destroy(vm);
}

static void test_clean_page_not_written_back(void)
{
    Config cfg;
    config_set_defaults(&cfg);
    cfg.verbosity = VERBOSITY_QUIET;
    VirtualMemory *vm = NULL;
    CHECK(vm_create(&vm, &cfg) == VM_OK);
    uint32_t base;
    uint8_t v;
    CHECK(vm_alloc(vm, 65u * 4096u, &base) == VM_OK);
    for (uint32_t i = 0; i < 65; i++) { /* solo lecturas: páginas limpias */
        CHECK(vm_read(vm, i * 4096u, &v) == VM_OK && v == 0);
    }
    CHECK(vm_stats(vm)->replacements == 1 && vm_stats(vm)->swap_outs == 0);
    vm_destroy(vm);
}

int main(void)
{
    test_numparse();
    test_layout();
    test_page_table();
    test_regions();
    test_swap();
    test_fifo_order();
    test_parser();
    test_config();
    test_vm_eviction_integrity();
    test_clean_page_not_written_back();

    printf("Pruebas unitarias: %d verificaciones, %d fallos\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
