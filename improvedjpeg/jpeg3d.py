import os
import numpy as np
from scipy.fftpack import dct, idct


coeffs = []
zigzag_coeffs = []
W = []
X = []
Z = []

#Read from file:
def read_from_files():
    for i in range(0, 8):

        os.system("picojpeg.exe klatki/frame"+str(i)+".jpg frame.bmp frame")
        os.remove("frame.bmp")

        file = open("frame", "rb")
        coeffs.append(np.fromfile(file, dtype=np.int16))
        file.close()

    print("\n Read from file!")
    return


#DCT:
def DCT_function():
    for i in range(0, len(coeffs[0])):
    #for i in range(0, 1):

        temp = []

        for j in range(0, 8):
            temp.append(coeffs[j][i])
        #print(temp)
        temp = dct(temp)

        for j in range(0, 8):
            coeffs[j][i] = temp[j]

    print("DCT Done!")
    return


#IDCT:
def IDCT_function():
    for i in range(0, len(coeffs[0])):
    #for i in range(0, 1):

        temp = []

        for j in range(0, 8):
            temp.append(coeffs[j][i])

        temp = np.round(idct(temp)/16)
        #print(temp)
        for j in range(0, 8):
            coeffs[j][i] = temp[j]

    print("IDCT done!")
    return


def save_files():
    for i in range(0, 8):

        file = open("frame", "wb")
        file.write(coeffs[i])
        file.close()

        os.system("picojpegtobmp.exe klatki/frame"+str(i)+".jpg klatki/decode"+str(i)+".bmp frame")

    print("\n Saved!")
    return


def zigzag_block(variable, mode, bool): #mode 0:  matrix -> vector, 1: vector -> matrix

    result = []
    if bool == 0:
        N = 8 #8x8x8 cube
        D = 3 #3 dimensions
        T = (N-1)*D+1
        Vc = []
        Vs = []
        Lic = []
        Lis = []
        Lsc = []
        Lss = []
        Wc = []
        Ws = []
        Av = []
        Ci = []
        Cd = []
        Ci2 = []
        Cd2 = []
        Sig = []

        for i in range(1, N+1):
            Vc.append(i)
            Vs.append(i)
            Av.append(i)
            Cd.append(i)
        for i in range(N-1, 0, -1):
            Vc.append(i)

        lVc = len(Vc)

        for i in range(0, T):
            Lic.append(0)
            Lis.append(0)
            Lsc.append(0)
            Lss.append(0)

        for t in range(0, T):
            if t < N:
                Lic[T-t-1] = 1
            else:
                Lic[T-t-1] = t-N+2
            if t < N:
                Lsc[t] = lVc
            else:
                Lsc[t] = lVc-t+N-1
            if t < T-(N-1):
                Lis[t] = 1
            else:
                Lis[t] = t-(T-N)+1
            if t < N:
                Lss[t] = t + 1
            else:
                Lss[t] = N

        for i in range(0, N-2):
            Av.append(N)

        for i in range(N, 0, -1):
            Av.append(i)

        for i in range(0, N-1):
            Ci.append(1)

        for i in range(1, N+1):
            Ci.append(i)

        lCi = len(Ci)

        for i in range(0, N-1):
            Cd.append(N)

        d = 0

        for t in range(0, T):
            if t % 2 != 0:
                for i in range(Lic[t], Lsc[t]+1):
                    Wc.append(Vc[i-1])

                for i in range(Lis[t], Lss[t]+1):
                    Ws.append(Vs[i-1])

                if t < N:
                    for i in range(d+Av[t]-1, d-1, -1):
                        Ci2.append(Ci[i])

                    for i in range(d+Av[t]-1, d-1, -1):
                        Cd2.append(Cd[i])
                else:
                    d = d+1
                    for i in range(d+Av[t]-1, d-1, -1):
                        Ci2.append(Ci[i])

                    for i in range(d+Av[t]-1, d-1, -1):
                        Cd2.append(Cd[i])
            else:
                for i in range(Lsc[t], Lic[t]-1, -1):
                    Wc.append(Vc[i-1])
                for i in range(Lss[t], Lis[t]-1, -1):
                    Ws.append(Vs[i-1])

                if t < N:
                    for i in range(d, d+Av[t]):
                        Ci2.append(Ci[i])

                    for i in range(d, d+Av[t]):
                        Cd2.append(Cd[i])
                else:
                    d = d+1
                    for i in range(d, d+Av[t]):
                        Ci2.append(Ci[i])

                    for i in range(d, d+Av[t]):
                        Cd2.append(Cd[i])
            for i in range(0, Av[t]):
                Sig.append((-1) ** (t+1))

        L = len(Ws)
        #W = []
        #X = []
        #Z = []

        for l in range(0, L):
            for i in range(0, Wc[l]):
                W.append(Ws[l])

            if Sig[l] > 0:
                for i in range(Ci2[l], Cd2[l]+1):
                    X.append(i)
                for i in range(Cd2[l], Ci2[l]-1, -1):
                    Z.append(i)
            else:
                for i in range(Ci2[l], Cd2[l]+1):
                    Z.append(i)
                for i in range(Cd2[l], Ci2[l]-1, -1):
                    X.append(i)

    LL = len(W)

    if mode == 0:
        for i in range(0, LL):
            result.append(variable[W[i]-1][X[i]-1][Z[i]-1])
        return result
    else:
        for x in range(0, 8):
            result.append([])
            for y in range(0, 8):
                result[x].append([])
                for z in range(0, 8):
                    result[x][y].append(0)
        for i in range(0, LL):
            result[W[i]-1][X[i]-1][Z[i]-1] = variable[i]

        return result


def zigzag(blocks):

    bool = 0
    for i in range(0, blocks):

        mat = []

        for x in range(0, 8):
            mat.append([])
            for y in range(0, 8):
                mat[x].append([])
                for z in range(0, 8):
                    mat[x][y].append(coeffs[x][(y*8)+z])

        zigzag_coeffs.append(zigzag_block(mat, 0, bool))
        bool = 1

    print("Zig-zag done!")
    return


def izigzag(blocks):

    for i in range(0, blocks):

        mat = zigzag_block(zigzag_coeffs[i], 1, 1)

        for x in range(0, 8):
            for y in range(0, 8):
                for z in range(0, 8):
                    coeffs[x][(y*8)+z] = mat[x][y][z]

    print("Inverse zig-zag done!")
    return


read_from_files()
DCT_function()
zigzag(len(coeffs[0])//64)
izigzag(len(zigzag_coeffs))
IDCT_function()
save_files()

