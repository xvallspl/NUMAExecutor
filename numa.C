#include "TNUMAExecutor.hxx"
#include "ROOT/TSeq.hxx"
#include <iostream>

int main(){
   auto func = [&](long int i)->long int {
      long int x{}; 
      ROOT::TSeq<long int> s(100); 
      for(auto y: s) x+=y;
      return x;
   };
  
   auto func2 = []()->long int { 
      long int x{}; 
      ROOT::TSeq<long int> s(100); 
      for(auto y: s) x+=y;
      return x;    
   };

   auto redfunc = [](const std::vector<long int> &v){return std::accumulate(v.begin(), v.end(), 0l);};
  
   ROOT::Experimental::TNUMAExecutor n;

   auto res = n.MapReduce(func, ROOT::TSeq<long int>(200), redfunc, 23);
   std::cout<<"Checked TSeq overload..."<<std::endl;
  
   std::vector<long int> v(20000);
   auto res2 = n.MapReduce(func, v, redfunc, 23);
   std::cout<<"Checked vector overload..."<<std::endl;
  
   auto res3 = n.MapReduce(func2, 30, redfunc);
   std::cout<<"Checked times overload..."<<std::endl;
   return  res;
}
