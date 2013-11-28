set(SOURCES crypt.c
            guideclient.c
            GuideParser.cpp
            httpclient.c
            httpfsclient.c
            interface.c
            md5.c
            netclient.c
            rtv.c
            sleep.c)

add_options(ALL_LANGUAGES ALL_BUILDS "-fPIC")

add_library(rtv STATIC ${SOURCES})
