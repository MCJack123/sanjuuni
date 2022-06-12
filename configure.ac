AC_INIT([sanjuuni], [1.0])
AC_CONFIG_SRCDIR([src/sanjuuni.cpp])
m4_include([m4/find_cxx.m4])
m4_include([m4/find_cxx11.m4])

AC_LANG([C++])
CXXFLAGS="$CXXFLAGS -std=c++11"
AC_PROG_CC
AC_PROG_CXX
AX_CHECK_COMPILE_FLAG([-std=c++11], [], [AC_MSG_ERROR([C++ compiler does not support -std=c++11.])])

AC_CHECK_HEADERS([libavformat/avformat.h], [], [AC_MSG_ERROR([Could not find libavformat headers.])])
AC_CHECK_HEADERS([libavcodec/avcodec.h], [], [AC_MSG_ERROR([Could not find libavcodec headers.])])
AC_CHECK_HEADERS([libswscale/swscale.h], [], [AC_MSG_ERROR([Could not find libswscale headers.])])
AC_CHECK_HEADERS([libswresample/swresample.h], [], [AC_MSG_ERROR([Could not find libswresample headers.])])
AC_CHECK_HEADERS([zlib.h], [], [AC_MSG_ERROR([Could not find zlib headers.])])
AC_CHECK_HEADERS([Poco/Base64Encoder.h], [], [AC_MSG_ERROR([Could not find Poco Foundation headers.])])
AC_CHECK_HEADERS([Poco/Util/OptionProcessor.h], [], [AC_MSG_ERROR([Could not find Poco Util headers.])])
AC_CHECK_HEADERS([Poco/Net/HTTPServer.h], [], [AC_MSG_ERROR([Could not find Poco Net headers.])])
AC_CHECK_HEADERS([Poco/Net/HTTPSClientSession.h], [], [AC_MSG_ERROR([Could not find Poco NetSSL headers.])])
AC_CHECK_HEADERS([Poco/Crypto/X509Certificate.h], [], [AC_MSG_ERROR([Could not find Poco Crypto headers.])])

AC_SEARCH_LIBS(pthread_create, pthread, [], [AC_MSG_ERROR([Could not find pthread library.])])
AC_SEARCH_LIBS(avcodec_find_decoder, avcodec, [], [AC_MSG_ERROR([Could not find libavcodec library.])])
AC_SEARCH_LIBS(avformat_open_input, avformat, [], [AC_MSG_ERROR([Could not find libavformat library.])])
AC_SEARCH_LIBS(av_frame_alloc, avutil, [], [AC_MSG_ERROR([Could not find libavutil library.])])
AC_SEARCH_LIBS(sws_getContext, swscale, [], [AC_MSG_ERROR([Could not find libswscale library.])])
AC_SEARCH_LIBS(swr_alloc_set_opts, swresample, [], [AC_MSG_ERROR([Could not find libswresample library.])])
AC_SEARCH_LIBS(compress2, z, [], [AC_MSG_ERROR([Could not find zlib library.])])
AX_CXX_CHECK_LIB(PocoFoundation, [Poco::ASCIIEncoding], [], [AC_MSG_ERROR([Could not find Poco Foundation library.])])
AX_CXX_CHECK_LIB(PocoUtil, [Poco::Util::Timer], [], [AC_MSG_ERROR([Could not find Poco Util library.])])
AX_CXX_CHECK_LIB(PocoNet, [Poco::Net::HTTPRequest], [], [AC_MSG_ERROR([Could not find Poco Net library.])])
AX_CXX_CHECK_LIB(PocoNetSSL, [Poco::Net::HTTPSClientSession], [], [AC_MSG_ERROR([Could not find Poco NetSSL library.])])
AX_CXX_CHECK_LIB(PocoCrypto, [Poco::Crypto::Cipher], [], [AC_MSG_ERROR([Could not find Poco Crypto library.])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
