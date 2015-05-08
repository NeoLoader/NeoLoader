#if defined(WIN32) && defined(_DEBUG) 
 #define UDT_ASSERT(x)	if(!x) _CrtDbgBreak();
 #define UDT_TRACE(x)	_CrtDbgReport(0,__FILE__,__LINE__,"UDT","%s", x "\r\n");
 #define UDT_TRACE_1(x, _1)	_CrtDbgReport(0,__FILE__,__LINE__,"UDT", x "\r\n", _1);
 #define UDT_TRACE_2(x, _1, _2)	_CrtDbgReport(0,__FILE__,__LINE__,"UDT", x "\r\n", _1, _2);
 #define UDT_TRACE_3(x, _1, _2, _3)	_CrtDbgReport(0,__FILE__,__LINE__,"UDT", x "\r\n", _1, _2, _3);
 #define UDT_TRACE_4(x, _1, _2, _3, _4)	_CrtDbgReport(0,__FILE__,__LINE__,"UDT", x "\r\n", _1, _2, _3, _4);
 #define UDT_TRACE_5(x, _1, _2, _3, _4, _5)	_CrtDbgReport(0,__FILE__,__LINE__,"UDT", x "\r\n", _1, _2, _3, _4, _5);
#else
 #define UDT_ASSERT(x)
 #define UDT_TRACE(x)
 #define UDT_TRACE_1(x, _1)
 #define UDT_TRACE_2(x, _1, _2)
 #define UDT_TRACE_3(x, _1, _2, _3)
 #define UDT_TRACE_4(x, _1, _2, _3, _4)
 #define UDT_TRACE_5(x, _1, _2, _3, _4, _5)
#endif