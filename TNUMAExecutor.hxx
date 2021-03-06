#include <ROOT/TProcessExecutor.hxx>
#include <ROOT/TThreadExecutor.hxx>
#include "ROOT/RArrayView.hxx" // std::array_view

#include <numa.h>
#include <algorithm> // std::min, std::max

namespace ROOT {
namespace Experimental {

class TNUMAExecutor {
public:

   template< class F, class... T>
   using noReferenceCond = typename std::enable_if<"Function can't return a reference" && !(std::is_reference<typename std::result_of<F(T...)>::type>::value)>::type;

   explicit TNUMAExecutor():fNDomains(numa_max_node()+1){}

   unsigned GetNUMADomains(){
      return fNDomains;
   }

   template<class F, class R, class Cond = noReferenceCond<F>>
   auto MapReduce(F func, unsigned nTimes, R redfunc, unsigned nChunks = 0) -> typename std::result_of<F()>::type;
   template<class F, class INTEGER, class R, class Cond = noReferenceCond<F, INTEGER>>
   auto MapReduce(F func, ROOT::TSeq<INTEGER> args, R redfunc, unsigned nChunks = 0) -> typename std::result_of<F(INTEGER)>::type;
   template<class F, class T, class R, class Cond = noReferenceCond<F, T>>
   auto MapReduce(F func, std::initializer_list<T> args, R redfunc, unsigned nChunks = 0) -> typename std::result_of<F(T)>::type
   {
      std::vector<T> vargs(std::move(args));
      return MapReduce(func, vargs, redfunc, nChunks);
   }
   template<class F, class T, class R, class Cond = noReferenceCond<F, T>>
   auto MapReduce(F func, std::vector<T> &args, R redfunc, unsigned nChunks = 0) -> typename std::result_of<F(T)>::type;

private:

   template<class T>
   std::vector<std::array_view<T>> splitData(std::vector<T> &vec);

   unsigned fNDomains{};
};


template<class T>
std::vector<std::array_view<T>> TNUMAExecutor::splitData(std::vector<T> &vec)
{
   unsigned int nToProcess = vec.size();
   unsigned stride = (nToProcess + fNDomains - 1) / fNDomains; //ceiling the division
   auto av = std::make_view(vec);
   std::vector<std::array_view<T>> v;
   unsigned i;
   for(i=0; i*stride<av.size()-stride; i++) {
      v.emplace_back(av.slice(av.begin()+ i*stride, av.begin()+(i+1)*stride));
   }
   v.emplace_back(av.slice(av.begin() + i*stride, av.end()));

   return v;
}

template<class F, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, unsigned nTimes, R redfunc, unsigned nChunks) -> typename std::result_of<F()>::type
{
   auto runOnNode = [&](unsigned int i) {
      numa_run_on_node(i);
      ROOT::TThreadExecutor pool;
      auto res = nChunks? pool.MapReduce(func, nTimes, redfunc, nChunks) : pool.MapReduce(func, nTimes, redfunc); 
      numa_run_on_node_mask(numa_all_nodes_ptr);
      return res;
   };

   ROOT::TProcessExecutor proc(fNDomains);
   return proc.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
}

template<class F, class T, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, std::vector<T> &args, R redfunc, unsigned nChunks) -> typename std::result_of<F(T)>::type
{
   auto dataRanges = splitData(args);
   auto runOnNode = [&](unsigned int i) {
      numa_run_on_node(i);
      ROOT::TThreadExecutor pool;
      auto res = nChunks? pool.MapReduce(func, dataRanges[i], redfunc, nChunks) : pool.MapReduce(func, dataRanges[i], redfunc); 
      numa_run_on_node_mask(numa_all_nodes_ptr);
      return res;
   };

   ROOT::TProcessExecutor proc(fNDomains);
   return proc.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
}



template<class F, class INTEGER, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, ROOT::TSeq<INTEGER> args, R redfunc, unsigned nChunks) -> typename std::result_of<F(INTEGER)>::type
{
   unsigned stride = (*args.end() - *args.begin() + fNDomains - 1) / fNDomains; //ceiling the division
   
   auto runOnNode = [&](INTEGER i) {
      numa_run_on_node(i);
      ROOT::TThreadExecutor pool;
      ROOT::TSeq<unsigned> sequence(std::max(*args.begin(), i*stride), std::min((i+1)*stride, *args.end()));
      auto res = nChunks? pool.MapReduce(func, sequence, redfunc, nChunks) : pool.MapReduce(func, sequence, redfunc); 
      numa_run_on_node_mask(numa_all_nodes_ptr);
      return res;
   };

   ROOT::TProcessExecutor proc(fNDomains);
   return proc.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
}

} // namespace Experimental
} // namespace ROOT
