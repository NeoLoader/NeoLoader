#pragma once

#include <stdint.h>

namespace boost
{

  using ::int8_t;             
  using ::int_least8_t;       
  using ::int_fast8_t;        
  using ::uint8_t;            
  using ::uint_least8_t;      
  using ::uint_fast8_t;       
                     
  using ::int16_t;            
  using ::int_least16_t;      
  using ::int_fast16_t;       
  using ::uint16_t;           
  using ::uint_least16_t;     
  using ::uint_fast16_t;      
                     
  using ::int32_t;            
  using ::int_least32_t;      
  using ::int_fast32_t;       
  using ::uint32_t;           
  using ::uint_least32_t;     
  using ::uint_fast32_t;      
                     
# ifndef BOOST_NO_INT64_T

  using ::int64_t;            
  using ::int_least64_t;      
  using ::int_fast64_t;       
  using ::uint64_t;           
  using ::uint_least64_t;     
  using ::uint_fast64_t;      
                     
# endif

  using ::intmax_t;      
  using ::uintmax_t;     

  //class noncopyable
  //{
  // protected:
  //    noncopyable() {}
  //    ~noncopyable() {}
  // private:  // emphasize the following members are private
  //    noncopyable( const noncopyable& );
  //    const noncopyable& operator=( const noncopyable& );
  //};

} // namespace boost


#include <string>
#include <sstream>
#include <deque>
#include <list>
#include <vector>
#include <map>
#include <set>

#include "../Framework/Types.h"
