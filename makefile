#NB_REVISION#

#NB_COPYRIGHT#

NAME    = SecureDIPSwitches

CPP_SRC     += \
            src/main.cpp \
            src/SimpleAD.cpp

CPP_SRC += \
			src/key.cpp \
			src/cert.cpp \
			src/htmldata.cpp

CREATEDTARGS += \
			src/key.cpp \
			src/cert.cpp \
			src/htmldata.cpp

src/htmldata.cpp : $(wildcard html/*.*)
	comphtml html -osrc/htmldata.cpp

device.crt: device.key

device.key: CA.crt
	openssl genrsa -out device.key 1024
	openssl req -new -config ssl_cert_config.txt -key device.key -out device.csr
	openssl x509 -req -days 3650 -in device.csr -CA CA.crt -CAkey CA.key -CAcreateserial -out device.crt

CA.key:
	openssl genrsa -out CA.key 1024

CA.crt: CA.key
	openssl genrsa -out CA.key 1024
	openssl req -new -config ssl_cert_config.txt -key CA.key -x509 -days 3650 -out CA.crt

src/key.cpp: device.key
	compfile device.key comp_key comp_key_len $@

src/cert.cpp: device.crt
	compfile device.crt comp_cert comp_cert_len $@


include $(NNDK_ROOT)/make/boilerplate.mk
