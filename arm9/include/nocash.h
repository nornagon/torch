#ifndef NOCASH_H
#define NOCASH_H 1

#define NC_DEBUG(msg) asm volatile (\
		"mov r12,r12\n" \
		"b 2f\n" \
		".hword 0x6464\n" \
		".hword 0\n" \
		".ascii \"" msg "\"\n" \
		".byte 0\n" \
		".align 4\n" \
		"2:\n" \
		)

#endif /* NOCASH_H */
