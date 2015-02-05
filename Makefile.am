AUTOMAKE_OPTIONS=foreign

bin_PROGRAMS = memwared

memwared_SOURCES = memwared.c memwared.h \
				   hash.c hash.h \
				   jenkins_hash.c jenkins_hash.h \
				   murmur3_hash.c murmur3_hash.h \
				   #assoc.c assoc.h 

memwared_LDFLAGS = -lmongoc-1.0

INCLUDES = -I/root/source/mongo-c-driver-1.1.0/src/libbson/src/bson -I/root/source/mongo-c-driver-1.1.0/src/mongoc
