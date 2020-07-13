import os
import numpy as np


os.system("picojpeg.exe image1.jpg image1.bmp coeffs1")
os.system("picojpeg.exe image2.jpg image2.bmp coeffs2")

coeffs1 = open("coeffs1", "rb")
coeffs2 = open("coeffs2", "rb")


buf1 = np.fromfile(coeffs1, dtype=np.int16)
buf2 = np.fromfile(coeffs2, dtype=np.int16)

coeffs2.close()

diffbuf = buf2 - buf1

diff = open("diff", "wb")
diff.write(diffbuf)


diff.close()

os.system("toojpeg.exe diff diff.jpg 0")


os.system("picojpeg.exe diff.jpg diff.bmp diff")


diff = open("diff", "rb")

buf2 = np.fromfile(diff, dtype=np.int16)

diff.close()


diffbuf = buf1 + buf2

diff = open("recover_diff", "wb")

diff.write(diffbuf)

diff.close()

os.system("picojpegtobmp.exe diff.jpg recovered.bmp recover_diff")


os.system("toojpeg.exe coeffs2 im2compress.jpg 0")

