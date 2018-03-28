#include <isl/set.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/ast_build.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>

#include <tiramisu/debug.h>
#include <tiramisu/core.h>

#include <string.h>
#include <Halide.h>
#include "halide_image_io.h"


using namespace tiramisu;

constexpr auto it_type = p_int32;
constexpr auto data_type = p_uint8;
constexpr auto work_type = p_float32;
constexpr auto block_size = 16;
constexpr auto block_size_i = 16;
#define BLOCK_SIZE_I "16"
constexpr auto block_size_j = 11;
#define BLOCK_SIZE_J "11"
constexpr auto kern_size = 5;

#define MAKE_BIGGER_LOOP(name, str_name, e) computation name{"[N, M, C] -> {" str_name "[i, j0, j1, c]: 0 <= i < N - 5 and 0 <= j0 * " BLOCK_SIZE_J " < M - 5 and 0 <= j1 < " BLOCK_SIZE_J " + 5 and 0 <= j0 * " BLOCK_SIZE_J " + j1 < M and 0 <= c < C}", (e), true, data_type, &gaussian_tiramisu};\
                            (name).split(i, block_size_i, i0, i1); \
                            (name).interchange(i1, j0)

int main(int argc, char **argv)
{
    // Set default tiramisu options.
    global::set_default_tiramisu_options();
    global::set_loop_iterator_type(it_type);

    tiramisu::function gaussian_tiramisu("gaussiangpu_tiramisu");

    var i("i"), j("j"), i0("i0"), j0("j0"), i1("i1"), j1("j1"), c("c");

    computation sizes{"{sizes[i]: 0 <= i < 3}", expr(), false, it_type, &gaussian_tiramisu};
    constant N{"N", sizes(0), it_type, true, nullptr, computation::root_dimension, &gaussian_tiramisu};
    constant M{"M", sizes(1), it_type, true, nullptr, computation::root_dimension, &gaussian_tiramisu};
    constant C{"C", sizes(2), it_type, true, nullptr, computation::root_dimension, &gaussian_tiramisu};
    computation gpu_input{"[N, M, C] -> {gpu_input[i, j, c]: 0 <= i < N and 0 <= j < M and 0 <= c < C}", expr(), false, data_type, &gaussian_tiramisu};
    computation shared_x{"[N, M] -> {shared_x[i, o_i, j1]: 0 <= i < N and 0 <= j1 < " BLOCK_SIZE_J " + 5 and 0 <= o_i < 5}", expr(), false, data_type, &gaussian_tiramisu};
    computation shared_y{"[N, M] -> {shared_y[i, j, o_j]: 0 <= i < N and 0 <= j < M and 0 <= o_j < 5}", expr(), false, data_type, &gaussian_tiramisu};
    computation kernel_x{"{kernel_x[i]: 0 <= i < 5}", expr(), false, work_type, &gaussian_tiramisu};
    computation kernel_y{"{kernel_y[i]: 0 <= i < 5}", expr(), false, work_type, &gaussian_tiramisu};

    tiramisu::expr e = value_cast(work_type, 0);
    for (int k = 0; k < 5; k++)
        e = e + cast(work_type, shared_x(i, k, j1)) * kernel_x(k);
    e = cast(data_type, e);
    MAKE_BIGGER_LOOP(gaussian_x, "gaussian_x", e);

    std::cout << isl_set_to_str(gaussian_x.get_iteration_domain()) << std::endl;

    e = value_cast(work_type, 0);
    for (int k = 0; k < 5; k++)
        e = e + cast(work_type, shared_y(i, j, k)) * kernel_y(k);
    e = cast(data_type, e);
    computation gaussian_y{"[N, M, C] -> {gaussian_y[i, j, c]: 0 <= i < N - 5 and 0 <= j < M - 5 and 0 <= c < C}", e, true, data_type, &gaussian_tiramisu};


    gaussian_y.tile(i, j, block_size_i, block_size_j, i0, j0, i1, j1);

    gaussian_x.before(gaussian_y, c);
    gaussian_x.tag_gpu_level(i0, j0, i1, j1);


    buffer sizes_b{"sizes_b", {3}, it_type, a_input, &gaussian_tiramisu};
    buffer input{"input", {N, M, C}, data_type, a_input, &gaussian_tiramisu};
    buffer output{"output", {N - 5, M - 5, C}, data_type, a_output, &gaussian_tiramisu};
    sizes.set_access("{sizes[i] -> sizes_b[i]}");
    buffer kxo{"kxo", {5}, work_type, a_input, &gaussian_tiramisu};
    buffer kx{"kx", {5}, work_type, a_temporary, &gaussian_tiramisu};
    kx.tag_gpu_constant();
    buffer kyo{"kyo", {5}, work_type, a_input, &gaussian_tiramisu};
    buffer ky{"ky", {5}, work_type, a_temporary, &gaussian_tiramisu};
    ky.tag_gpu_constant();
    kernel_x.set_access("{kernel_x[i] -> kx[i]}");
    kernel_y.set_access("{kernel_y[i] -> ky[i]}");

    buffer sxbuf{"sxbuf", {block_size + 5, block_size + 5}, data_type, a_temporary, &gaussian_tiramisu};
    sxbuf.tag_gpu_shared();
    buffer sybuf{"sybuf", {block_size_i + 5, block_size_j}, data_type, a_temporary, &gaussian_tiramisu};
    sybuf.tag_gpu_shared();
    shared_x.set_access("{shared_x[i, o_i, j1] -> sxbuf[i % " BLOCK_SIZE_I " + o_i, j1]}");
    shared_y.set_access("{shared_y[i, j, o_j] -> sybuf[i % " BLOCK_SIZE_I ", j % " BLOCK_SIZE_J " + o_j]}");

    buffer gpu_in{"gpu_in", {N, M, C}, data_type, a_temporary, &gaussian_tiramisu};
    gpu_in.tag_gpu_global();
    gpu_input.set_access("{gpu_input[i, j, c] -> gpu_in[i, j, c]}");
    buffer gpu_out{"gpu_out", {N - 5, M - 5, C}, data_type, a_temporary, &gaussian_tiramisu};
    gpu_out.tag_gpu_global();

    gaussian_x.set_access("{gaussian_x[i, j0, j1, c] -> sybuf[i % " BLOCK_SIZE_I ", j1]}");
    gaussian_y.set_access("{gaussian_y[i, j, c] -> gpu_out[i, j, c]}");


    MAKE_BIGGER_LOOP(shared_x_dec, "shared_x_dec", expr(o_allocate, sxbuf.get_name()));
    MAKE_BIGGER_LOOP(shared_y_dec, "shared_y_dec", expr(o_allocate, sybuf.get_name()));

    shared_x_dec.tag_gpu_level(i0, j0, i1, j1);

    shared_y_dec.after(shared_x_dec, c);
    shared_y_dec.before(gaussian_x, c);

    MAKE_BIGGER_LOOP(copy_to_shared, "copy_to_shared", gpu_input(i, j0 * block_size_j + j1, c));
    copy_to_shared.set_access("{copy_to_shared[i, j0, j1, c] -> sxbuf[i % " BLOCK_SIZE_I", j1]}");

    MAKE_BIGGER_LOOP(copy_to_shared_bl, "copy_to_shared_bl", gpu_input(i + block_size_i, j0 * block_size_j + j1, c));
    copy_to_shared_bl.set_access("{copy_to_shared_bl[i, j0, j1, c] -> sxbuf[i % " BLOCK_SIZE_I" + " BLOCK_SIZE_I ", j1]}");
    copy_to_shared_bl.add_predicate(i % block_size_i < 5);

    MAKE_BIGGER_LOOP(copy_to_shared_tr, "copy_to_shared_tr", gpu_input(i, (j0 + 1) * block_size_j + j1, c));
    copy_to_shared_tr.set_access("{copy_to_shared_tr[i, j0, j1, c] -> sxbuf[i % " BLOCK_SIZE_I", j1 + " BLOCK_SIZE_J"]}");
    copy_to_shared_tr.add_predicate(j1 < 5);

    MAKE_BIGGER_LOOP(copy_to_shared_br, "copy_to_shared_br", gpu_input(i + block_size_i, j0 * block_size_j + j1, c));
    copy_to_shared_br.set_access("{copy_to_shared_br[i, j0, j1, c] -> sxbuf[i % " BLOCK_SIZE_I" + " BLOCK_SIZE_I ", j1 + " BLOCK_SIZE_J "]}");
    copy_to_shared_br.add_predicate(i % block_size_i < 5 && j1 < 5);

    copy_to_shared.between(shared_y_dec, c, gaussian_x, c);
    copy_to_shared_tr.between(copy_to_shared, c, gaussian_x, c);
    copy_to_shared_br.between(copy_to_shared_tr, c, gaussian_x, c);
    copy_to_shared_bl.between(copy_to_shared_br, c, gaussian_x, c);

    MAKE_BIGGER_LOOP(synchronize1, "synchronize1", tiramisu::sync{});
    MAKE_BIGGER_LOOP(synchronize2, "synchronize2", tiramisu::sync{});
    MAKE_BIGGER_LOOP(synchronize3, "synchronize3", tiramisu::sync{});

    synchronize1.between(copy_to_shared_bl, c, gaussian_x, c);
    synchronize2.between(gaussian_x, c, gaussian_y, c);

    computation copy_to_gpu_in{"{copy_to_gpu_in[0]}", expr(o_memcpy, var(input.get_name()), var(gpu_in.get_name())), true, p_none, &gaussian_tiramisu};
    computation copy_kx{"{copy_kx[0]}", expr(o_memcpy, var(kxo.get_name()), var(kx.get_name())), true, p_none, &gaussian_tiramisu};
    computation copy_ky{"{copy_ky[0]}", expr(o_memcpy, var(kyo.get_name()), var(ky.get_name())), true, p_none, &gaussian_tiramisu};
    computation copy_to_out{"{copy_to_out[0]}", expr(o_memcpy, var(gpu_out.get_name()), var(output.get_name())), true, p_none, &gaussian_tiramisu};

    copy_kx.between(copy_to_gpu_in, computation::root, copy_ky, computation::root);
    copy_ky.before(shared_x_dec, computation::root);
    synchronize3.between(gaussian_y, c, copy_to_out, computation::root);


    gaussian_tiramisu.set_arguments({&sizes_b, &input, &kxo, &kyo, &output});
    gaussian_tiramisu.gen_time_space_domain();
    gaussian_tiramisu.gen_isl_ast();
    gaussian_tiramisu.gen_c_code();
    gaussian_tiramisu.gen_cuda_stmt();
    gaussian_tiramisu.gen_halide_stmt();
    gaussian_tiramisu.dump_halide_stmt();
    gaussian_tiramisu.gen_halide_obj("build/generated_fct_gaussiangpu.o");

    return 0;
}
