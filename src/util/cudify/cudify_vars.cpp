#include "config.h"
#include "cudify_hardware_common.hpp"
#ifdef HAVE_BOOST_CONTEXT
#include <boost/context/continuation.hpp>
#endif
#include <vector>
#include "cudify_hardware_common.hpp"

#ifdef HAVE_ALPAKA
#include "cudify_hardware_alpaka.hpp"

alpa_base_structs __alpa_base__;
#endif


thread_local int vct_atomic_add;
thread_local int vct_atomic_rem;

size_t n_workers = 1;

#ifdef HAVE_BOOST_CONTEXT
std::vector<void *> mem_stack;

std::vector<boost::context::detail::fcontext_t> contexts;
thread_local void * par_glob;
thread_local boost::context::detail::fcontext_t main_ctx;
#endif
