AC_INIT([sanjuuni], [1.0])
AC_CONFIG_SRCDIR([src/sanjuuni.cpp])
m4_include([m4/find_cxx.m4])
m4_include([m4/find_cxx11.m4])

AC_LANG([C++])
CXXFLAGS="$CXXFLAGS -std=c++17"
AC_PROG_CC
AC_PROG_CXX
AX_CHECK_COMPILE_FLAG([-std=c++17], [], [AC_MSG_ERROR([C++ compiler does not support -std=c++17.])])

AC_CHECK_HEADERS([libavformat/avformat.h], [], [AC_MSG_ERROR([Could not find libavformat headers.])])
AC_CHECK_HEADERS([libavcodec/avcodec.h], [], [AC_MSG_ERROR([Could not find libavcodec headers.])])
AC_CHECK_HEADERS([libavdevice/avdevice.h], [], [AC_MSG_ERROR([Could not find libavdevice headers.])])
AC_CHECK_HEADERS([libavfilter/avfilter.h], [], [AC_MSG_ERROR([Could not find libavfilter headers.])])
AC_CHECK_HEADERS([libswscale/swscale.h], [], [AC_MSG_ERROR([Could not find libswscale headers.])])
AC_CHECK_HEADERS([libswresample/swresample.h], [], [AC_MSG_ERROR([Could not find libswresample headers.])])
AC_CHECK_HEADERS([zlib.h], [], [AC_MSG_ERROR([Could not find zlib headers.])])
AC_CHECK_HEADERS([Poco/Base64Encoder.h], [], [AC_MSG_ERROR([Could not find Poco Foundation headers.])])
AC_CHECK_HEADERS([Poco/Util/OptionProcessor.h], [], [AC_MSG_ERROR([Could not find Poco Util headers.])])
AC_CHECK_HEADERS([Poco/Net/HTTPServer.h], [], [NO_NET=1])
AC_CHECK_HEADERS([Poco/Net/HTTPSClientSession.h], [], [NO_NET=1])
AC_CHECK_HEADERS([Poco/Crypto/X509Certificate.h], [], [NO_NET=1])
AC_CHECK_HEADERS([CL/opencl.hpp], [], [HAS_OPENCL=0])

AC_SEARCH_LIBS(pthread_create, pthread, [], [AC_MSG_ERROR([Could not find pthread library.])])
AC_SEARCH_LIBS(avcodec_find_decoder, avcodec, [], [AC_MSG_ERROR([Could not find libavcodec library.])])
AC_SEARCH_LIBS(avdevice_register_all, avdevice, [], [AC_MSG_ERROR([Could not find libavdevice library.])])
AC_SEARCH_LIBS(avfilter_version, avfilter, [], [AC_MSG_ERROR([Could not find libavfilter library.])])
AC_SEARCH_LIBS(avformat_open_input, avformat, [], [AC_MSG_ERROR([Could not find libavformat library.])])
AC_SEARCH_LIBS(av_frame_alloc, avutil, [], [AC_MSG_ERROR([Could not find libavutil library.])])
AC_SEARCH_LIBS(sws_getContext, swscale, [], [AC_MSG_ERROR([Could not find libswscale library.])])
AC_SEARCH_LIBS(swr_alloc_set_opts, swresample, [], [AC_MSG_ERROR([Could not find libswresample library.])])
AC_SEARCH_LIBS(compress2, z, [], [AC_MSG_ERROR([Could not find zlib library.])])
AX_CXX_CHECK_LIB(PocoFoundation, [Poco::ASCIIEncoding], [], [AC_MSG_ERROR([Could not find Poco Foundation library.])])
AX_CXX_CHECK_LIB(PocoUtil, [Poco::Util::Timer], [], [AC_MSG_ERROR([Could not find Poco Util library.])])
if test "x$NO_NET" != "x1"; then
    AX_CXX_CHECK_LIB(PocoNet, [Poco::Net::HTTPRequest], [], [AC_MSG_ERROR([Could not find Poco Net library.])])
    AX_CXX_CHECK_LIB(PocoNetSSL, [Poco::Net::HTTPSClientSession], [], [AC_MSG_ERROR([Could not find Poco NetSSL library.])])
    AX_CXX_CHECK_LIB(PocoCrypto, [Poco::Crypto::OpenSSLInitializer], [], [AC_MSG_ERROR([Could not find Poco Crypto library.])])
fi
if test "x$HAS_OPENCL" != "x0"; then
    AC_SEARCH_LIBS(clCreateContext, OpenCL, [HAS_OPENCL=1], [HAS_OPENCL=0])
fi
if test "x$HAS_OPENCL" = "x1"; then CPPFLAGS="$CPPFLAGS -DHAS_OPENCL"; fi
if test "x$NO_NET" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_NET"; fi

AC_CACHE_CHECK([for GNU sed], [ac_cv_path_SED], [AC_PATH_PROGS_FEATURE_CHECK([SED], [sed gsed], [[$ac_path_SED --version > /dev/null 2>/dev/null && ac_cv_path_SED=$ac_path_SED ac_path_SED_found=:]], [AC_MSG_ERROR([Could not find GNU sed. On Mac try brew install gsed.])])])
AC_SUBST([SED], [$ac_cv_path_SED])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
