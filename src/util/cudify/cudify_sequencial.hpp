#ifndef CUDIFY_SEQUENCIAL_HPP_
#define CUDIFY_SEQUENCIAL_HPP_

#ifdef BOOST_HAVE_CONTEXT

#define CUDIFY_ACTiVE

#include "util/cuda_util.hpp"
#include "boost/bind.hpp"
#include <type_traits>
#include "cudify_hardware_common.hpp"
#ifdef HAVE_BOOST_CONTEXT
#include <boost/context/continuation.hpp>
#endif
#include <vector>
#include <string.h>


#ifndef CUDIFY_BOOST_CONTEXT_STACK_SIZE
#define CUDIFY_BOOST_CONTEXT_STACK_SIZE 8192
#endif

extern std::vector<void *>mem_stack;

extern dim3 threadIdx;
extern dim3 blockIdx;

extern dim3 blockDim;
extern dim3 gridDim;

extern std::vector<void *> mem_stack;
extern std::vector<boost::context::detail::fcontext_t> contexts;
extern void * par_glob;
extern boost::context::detail::fcontext_t main_ctx;

static void __syncthreads()
{
    boost::context::detail::jump_fcontext(main_ctx,par_glob);
};

static void cudaDeviceSynchronize()
{}

static void cudaMemcpyFromSymbol(void * dev_mem,const unsigned char * global_cuda_error_array,size_t sz)
{
    memcpy(dev_mem,global_cuda_error_array,sz);
}

/**
 * CUDA memory copy types
 */
enum  cudaMemcpyKind
{
    cudaMemcpyHostToHost          =   0,      /**< Host   -> Host */
    cudaMemcpyHostToDevice        =   1,      /**< Host   -> Device */
    cudaMemcpyDeviceToHost        =   2,      /**< Device -> Host */
    cudaMemcpyDeviceToDevice      =   3,      /**< Device -> Device */
    cudaMemcpyDefault             =   4       /**< Direction of the transfer is inferred from the pointer values. Requires unified virtual addressing */
};

extern int vct_atomic_add;
extern int vct_atomic_rem;

static void cudaMemcpyToSymbol(unsigned char * global_cuda_error_array,const void * mem,size_t sz,int offset,int unused)
{
    memcpy(global_cuda_error_array+offset,mem,sz);
}

namespace cub
{
    template<typename T, unsigned int dim>
    class BlockScan
    {
        public: 
        typedef std::array<T,dim> TempStorage;

        private:
        TempStorage & tmp;

        public:

        

        BlockScan(TempStorage & tmp)
        :tmp(tmp)
        {};

        void ExclusiveSum(T & in, T & out)
        {
            tmp[threadIdx.x] = in;

            __syncthreads();

            if (threadIdx.x == 0)
            {
                T prec = tmp[0];
                tmp[0] = 0;
                for (int i = 1 ; i < dim ; i++)
                {
                    auto next = tmp[i-1] + prec;
                    prec = tmp[i];
                    tmp[i] = next;
                }
            }

            __syncthreads();

            out = tmp[threadIdx.x];
            return;
        }
    };
}


template<typename T, typename T2>
static T atomicAdd(T * address, T2 val)  
{
    T old = *address;
    *address += val;
    return old;
};

#define MGPU_HOST_DEVICE

namespace mgpu
{
    template<typename type_t>
    struct less_t : public std::binary_function<type_t, type_t, bool> {
    bool operator()(type_t a, type_t b) const {
        return a < b;
    }
    template<typename type2_t, typename type3_t>
    bool operator()(type2_t a, type3_t b) const {
        return a < b;
    }
    };
/*    template<typename type_t>
    struct less_equal_t : public std::binary_function<type_t, type_t, bool> {
    MGPU_HOST_DEVICE bool operator()(type_t a, type_t b) const {
        return a <= b;
    }
    };*/
    template<typename type_t>
    struct greater_t : public std::binary_function<type_t, type_t, bool> {
    MGPU_HOST_DEVICE bool operator()(type_t a, type_t b) const {
        return a > b;
    }
    template<typename type2_t, typename type3_t>
    MGPU_HOST_DEVICE bool operator()(type2_t a, type3_t b) const {
        return a > b;
    }
    };
/*    template<typename type_t>
    struct greater_equal_t : public std::binary_function<type_t, type_t, bool> {
    MGPU_HOST_DEVICE bool operator()(type_t a, type_t b) const {
        return a >= b;
    }
    };
    template<typename type_t>
    struct equal_to_t : public std::binary_function<type_t, type_t, bool> {
    MGPU_HOST_DEVICE bool operator()(type_t a, type_t b) const {
        return a == b;
    }
    };
    template<typename type_t>
    struct not_equal_to_t : public std::binary_function<type_t, type_t, bool> {
    MGPU_HOST_DEVICE bool operator()(type_t a, type_t b) const {
        return a != b;
    }
    };*/

    ////////////////////////////////////////////////////////////////////////////////
    // Device-side arithmetic operators.

    template<typename type_t>
    struct plus_t : public std::binary_function<type_t, type_t, type_t> {
        type_t operator()(type_t a, type_t b) const {
        return a + b;
    }
    };

/*    template<typename type_t>
    struct minus_t : public std::binary_function<type_t, type_t, type_t> {
        MGPU_HOST_DEVICE type_t operator()(type_t a, type_t b) const {
        return a - b;
    }
    };

    template<typename type_t>
    struct multiplies_t : public std::binary_function<type_t, type_t, type_t> {
    MGPU_HOST_DEVICE type_t operator()(type_t a, type_t b) const {
        return a * b;
    }
    };*/

    template<typename type_t>
    struct maximum_t  : public std::binary_function<type_t, type_t, type_t> {
    type_t operator()(type_t a, type_t b) const {
        return std::max(a, b);
    }
    };

    template<typename type_t>
    struct minimum_t  : public std::binary_function<type_t, type_t, type_t> {
    type_t operator()(type_t a, type_t b) const {
        return std::min(a, b);
    }
    };
}


namespace mgpu
{
    template<typename input_it,
             typename segments_it, typename output_it, typename op_t, typename type_t, typename context_t>
    void segreduce(input_it input, int count, segments_it segments,
                    int num_segments, output_it output, op_t op, type_t init,
                    context_t& context)
    {
        int i = 0;
        for ( ; i < num_segments - 1; i++)
        {
            int j = segments[i];
            output[i] = input[j];
            ++j;
            for ( ; j < segments[i+1] ; j++)
            {
                output[i] = op(output[i],input[j]);
            }
        }

        // Last segment
        int j = segments[i];
        output[i] = input[j];
        ++j;
        for ( ; j < count ; j++)
        {
            output[i] = op(output[i],input[j]);
        }
    }

    // Key-value merge.
    template<typename a_keys_it, typename a_vals_it,
             typename b_keys_it, typename b_vals_it,
             typename c_keys_it, typename c_vals_it,
             typename comp_t, typename context_t>
    void merge(a_keys_it a_keys, a_vals_it a_vals, int a_count,
               b_keys_it b_keys, b_vals_it b_vals, int b_count,
            c_keys_it c_keys, c_vals_it c_vals, comp_t comp, context_t& context) 
    {
        int a_it = 0;
        int b_it = 0;
        int c_it = 0;

        while (a_it < a_count || b_it < b_count)
        {
            if (a_it < a_count)
            {
                if (b_it < b_count)
                {
                    if (comp(b_keys[b_it],a_keys[a_it]))
                    {
                        c_keys[c_it] = b_keys[b_it];
                        c_vals[c_it] = b_vals[b_it];
                        c_it++;
                        b_it++;
                    }
                    else
                    {
                        c_keys[c_it] = a_keys[a_it];
                        c_vals[c_it] = a_vals[a_it];
                        c_it++;
                        a_it++;
                    }
                }
                else
                {
                    c_keys[c_it] = a_keys[a_it];
                    c_vals[c_it] = a_vals[a_it];
                    c_it++;
                    a_it++;
                }
            }
            else
            {
                c_keys[c_it] = b_keys[b_it];
                c_vals[c_it] = b_vals[b_it];
                c_it++;
                b_it++;
            }
        }
    }
}

static void init_wrappers()
{}

template<typename lambda_f>
struct Fun_enc
{
    lambda_f Fn;

    Fun_enc(lambda_f Fn)
    :Fn(Fn)
    {}

    void run()
    {
        Fn();
    }
};

template<typename Fun_enc_type>
void launch_kernel(boost::context::detail::transfer_t par)
{
    main_ctx = par.fctx;
    par_glob = par.data;
    Fun_enc_type * ptr = (Fun_enc_type *)par.data;

    ptr->run();

    boost::context::detail::jump_fcontext(par.fctx,0);
}

template<typename lambda_f, typename ite_type>
static void exe_kernel(lambda_f f, ite_type & ite)
{
    if (mem_stack.size() < ite.nthrs())
    {
        int old_size = mem_stack.size();
        mem_stack.resize(ite.nthrs());

        for (int i = old_size ; i < mem_stack.size() ; i++)
        {
            mem_stack[i] = new char [8192];
        }
    }

    // Resize contexts
    contexts.resize(mem_stack.size());

    Fun_enc<lambda_f> fe(f);

    for (int i = 0 ; i < ite.wthr.z ; i++)
    {
        blockIdx.z = i;
        for (int j = 0 ; j < ite.wthr.y ; j++)
        {
            blockIdx.y = j;
            for (int k = 0 ; k < ite.wthr.x ; k++)
            {
                blockIdx.x = k;
                int nc = 0;
                for (int it = 0 ; it < ite.thr.z ; it++)
                {
                    for (int jt = 0 ; jt < ite.thr.y ; jt++)
                    {
                        for (int kt = 0 ; kt < ite.thr.x ; kt++)
                        {
                            contexts[nc] = boost::context::detail::make_fcontext((char *)mem_stack[nc]+CUDIFY_BOOST_CONTEXT_STACK_SIZE-16,CUDIFY_BOOST_CONTEXT_STACK_SIZE,launch_kernel<Fun_enc<lambda_f>>);;
                            nc++;
                        }
                    }
                }

                bool work_to_do = true;
                while(work_to_do)
                {
                    nc = 0;
                    // Work threads
                    for (int it = 0 ; it < ite.thr.z ; it++)
                    {
                        threadIdx.z = it;
                        for (int jt = 0 ; jt < ite.thr.y ; jt++)
                        {
                            threadIdx.y = jt;
                            for (int kt = 0 ; kt < ite.thr.x ; kt++)
                            {
                                threadIdx.x = kt;
                                auto t = boost::context::detail::jump_fcontext(contexts[nc],&fe);
                                contexts[nc] = t.fctx;
                                work_to_do &= (t.data != 0);
                                nc++;
                            }
                        }
                    }
                }
            }
        }
    }
}

template<typename lambda_f, typename ite_type>
static void exe_kernel_no_sync(lambda_f f, ite_type & ite)
{
    for (int i = 0 ; i < ite.wthr.z ; i++)
    {
        blockIdx.z = i;
        for (int j = 0 ; j < ite.wthr.y ; j++)
        {
            blockIdx.y = j;
            for (int k = 0 ; k < ite.wthr.x ; k++)
            {
                blockIdx.x = k;
                int fb = 0;
                // Work threads
                for (int it = 0 ; it < ite.wthr.z ; it++)
                {
                    threadIdx.z = it;
                    for (int jt = 0 ; jt < ite.wthr.y ; jt++)
                    {
                        threadIdx.y = jt;
                        for (int kt = 0 ; kt < ite.wthr.x ; kt++)
                        {
                            threadIdx.x = kt;
                            f();
                        }
                    }
                }
            }
        }
    }
}

#ifdef PRINT_CUDA_LAUNCHES

#define CUDA_LAUNCH(cuda_call,ite, ...)\
        \
        gridDim.x = ite.wthr.x;\
        gridDim.y = ite.wthr.y;\
        gridDim.z = ite.wthr.z;\
        \
        blockDim.x = ite.thr.x;\
        blockDim.y = ite.thr.y;\
        blockDim.z = ite.thr.z;\
        \
        CHECK_SE_CLASS1_PRE\
        \
        std::cout << "Launching: " << #cuda_call << std::endl;\
        \
        exe_kernel(\
        [&](boost::context::fiber && main) -> void {\
            \
            \
            main_fib = main;
\
            cuda_call(__VA_ARGS__);\
        },ite);\
        CHECK_SE_CLASS1_POST(#cuda_call,__VA_ARGS__)\
        }


#define CUDA_LAUNCH_DIM3(cuda_call,wthr_,thr_, ...)\
        {\
        dim3 wthr__(wthr_);\
        dim3 thr__(thr_);\
        \
        ite_gpu<1> itg;\
        itg.wthr = wthr;\
        itg.thr = thr;\
        \
        gridDim.x = wthr__.x;\
        gridDim.y = wthr__.y;\
        gridDim.z = wthr__.z;\
        \
        blockDim.x = thr__.x;\
        blockDim.y = thr__.y;\
        blockDim.z = thr__.z;\
        \
        CHECK_SE_CLASS1_PRE\
        std::cout << "Launching: " << #cuda_call << std::endl;\
        \
        exe_kernel(\
        [&] (boost::context::fiber && main) -> void {\
            \
            \
            main_fib = std::move(main);\
\
            cuda_call(__VA_ARGS__);\
            \
            return std::move(main_fib);\
            \
        });\
        CHECK_SE_CLASS1_POST(#cuda_call,__VA_ARGS__)\
        }

#define CUDA_CHECK()

#else

#define CUDA_LAUNCH(cuda_call,ite, ...) \
        {\
        gridDim.x = ite.wthr.x;\
        gridDim.y = ite.wthr.y;\
        gridDim.z = ite.wthr.z;\
        \
        blockDim.x = ite.thr.x;\
        blockDim.y = ite.thr.y;\
        blockDim.z = ite.thr.z;\
        \
        CHECK_SE_CLASS1_PRE\
        \
        exe_kernel([&]() -> void {\
        \
            \
            cuda_call(__VA_ARGS__);\
            \
            },ite);\
        \
        CHECK_SE_CLASS1_POST(#cuda_call,__VA_ARGS__)\
        }


#define CUDA_LAUNCH_DIM3(cuda_call,wthr_,thr_, ...)\
        {\
        dim3 wthr__(wthr_);\
        dim3 thr__(thr_);\
        \
        ite_gpu<1> itg;\
        itg.wthr = wthr_;\
        itg.thr = thr_;\
        \
        gridDim.x = wthr__.x;\
        gridDim.y = wthr__.y;\
        gridDim.z = wthr__.z;\
        \
        blockDim.x = thr__.x;\
        blockDim.y = thr__.y;\
        blockDim.z = thr__.z;\
        \
        CHECK_SE_CLASS1_PRE\
        \
        exe_kernel([&]() -> void {\
            \
            cuda_call(__VA_ARGS__);\
            \
            },itg);\
        \
        CHECK_SE_CLASS1_POST(#cuda_call,__VA_ARGS__)\
        }

#define CUDA_CHECK()

#endif

#endif

#endif