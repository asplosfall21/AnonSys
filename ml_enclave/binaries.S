.section .images, "a", %progbits
.globl images_bin
images_bin:
  .incbin "images.bin"

.section .labels, "a", %progbits
.global labels_bin
labels_bin:
  .incbin "labels.bin"
