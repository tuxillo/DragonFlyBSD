#
# Which version of config(8) is required.
%VERSREQ=	400026

.if !defined(S)
.if exists(./dragonfly/.)
S=	./dragonfly
.else
S=	../..
.endif
.endif

.include "$S/conf/kern.pre.mk"

#
# ARM64-specific linker command.
#
# Use a custom SYSTEM_LD to set the text segment start address via --defsym.
# The kernel is linked at virtual address (kernbase + SIZEOF_HEADERS) but
# loaded by the EFI loader at a physical address. The loader uses
# efi_translate() to handle the VA-to-PA conversion.
#
# Also strip ARM64 mapping symbols ($a, $d, $x) which are used by ELF
# toolchains but not needed in the final kernel.
#
SYSTEM_LD= @${CC} -nostdlib -ffreestanding -Wl,--hash-style=sysv \
	-Wl,-Bdynamic -Wl,-T,$S/platform/$P/conf/ldscript.$M \
	-Wl,--defsym='text_start=kernbase + SIZEOF_HEADERS' \
	-Wl,--export-dynamic -Wl,--dynamic-linker,/red/herring \
	-o ${.TARGET} -Wl,-X ${SYSTEM_OBJS} vers.o

SYSTEM_LD_TAIL= @${OBJCOPY} --strip-symbol gcc2_compiled. ${.TARGET} ; \
	${OBJCOPY} --wildcard --strip-symbol='$$[adtx]*' ${.TARGET} ; \
	${SIZE} ${.TARGET} ; chmod 755 ${.TARGET}

%BEFORE_DEPEND

%OBJS

%CFILES

%SFILES

%MFILES

%CLEAN

.include "$S/conf/kern.post.mk"

%RULES

# DO NOT DELETE THIS LINE -- make depend uses it
