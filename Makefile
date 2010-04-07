TARGETS=usut
CFLAGS+=-lusb-1.0 
CFLAGS+=-I/usr/local/include/libusb-1.0

${TARGETS}: ${TARGETS}.c
	${CC} ${CFLAGS} $< -o $@ 
clean:
	rm ${TARGETS}
