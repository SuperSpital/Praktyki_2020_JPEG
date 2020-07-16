import os
import numpy as np
import matplotlib.pyplot as plt

os.system("picojpeg.exe image1.jpg image1.bmp coeffs1")
os.system("picojpeg.exe image2.jpg image2.bmp coeffs2")

coeffs1 = open("coeffs1", "rb")
coeffs2 = open("diff", "rb")


buf1 = np.fromfile(coeffs1, dtype=np.int16)
buf2 = np.fromfile(coeffs2, dtype=np.int16)

coeffs1.close()
coeffs2.close()

for j in range(0, 64):

    temp = []

    for i in range(j, len(buf2), 64):
        temp.append(buf2[i])

    bins = int((max(temp) - min(temp))/5)

    if bins == 0:
        bins = 1

    plt.hist(temp, bins = bins)

    plt.xlim(-500, 500)
    plt.ylim(0, 50000)
    plt.title("Współczynnik nr "+str(j))
    plt.xlabel('Wartość współczynnika')
    plt.ylabel('Liczba wystąpień')

    #plt.show()
    plt.gcf().savefig("histogramy/hist"+str(j)+".png")
    plt.clf()
