from scipy.fftpack import dct, idct
import math

g1 = [0, 20, 3, 70, 1, 2, 3, 4]
g2 = [1, 10, 12, 0, 1, 2 ,0 ,4]

G1 = dct(g1)
G2 = dct(g2)
Y = dct(g1+g2)

print(G1)
print(G2)
print(Y)

skalars1 = [0.63764, 0.42234, 0.082721, 0.034065, 0.017667, 0.0099847, 0.0055557, 0.0025099]
skalars2 = [0.21531, 0.77023, 0.53912, 0.13445, 0.061717, 0.033207, 0.018050, 0.0080656]
skalars3 = [0.13258, 0.31383, 0.71850, 0.56678, 0.14999, 0.069782, 0.035717, 0.01554]
skalars4 = [0.098519, 0.21344, 0.28617, 0.70296, 0.57484, 0.1525, 0.067272, 0.027651]
skalars5 = []

Y0 = G1[0] + G2[0]
Y1 = skalars1[0]*(G1[0]-G2[0])+skalars1[1]*(G1[1]+G2[1])+skalars1[2]*(-G1[2]+G2[2])+skalars1[3]*(G1[3]+G2[3])+skalars1[4]*(-G1[4]+G2[4])+skalars1[5]*(G1[5]+G2[5])+skalars1[6]*(-G1[6]+G2[6])+skalars1[7]*(G1[7]+G2[7])
Y2 = G1[1] - G2[1]
Y3 = skalars2[0]*(-G1[0]+G2[0])+skalars2[1]*(G1[1]+G2[1])+skalars2[2]*(G1[2]-G2[2])+skalars2[3]*(-G1[3]-G2[3])+skalars2[4]*(G1[4]-G2[4])+skalars2[5]*(-G1[5]-G2[5])+skalars2[6]*(G1[6]-G2[6])+skalars2[7]*(-G1[7]-G2[7])
Y4 = G1[2] + G2[2]
Y5 = skalars3[0]*(G1[0]-G2[0])+skalars3[1]*(-G1[1]-G2[1])+skalars3[2]*(G1[2]-G2[2])+skalars3[3]*(G1[3]+G2[3])+skalars3[4]*(-G1[4]+G2[4])+skalars3[5]*(G1[5]+G2[5])+skalars3[6]*(-G1[6]+G2[6])+skalars3[7]*(G1[7]+G2[7])
Y6 = G1[3] - G2[3]
Y7 = skalars4[0]*(-G1[0]+G2[0])+skalars4[1]*(G1[1]+G2[1])+skalars4[2]*(-G1[2]+G2[2])+skalars4[3]*(G1[3]+G2[3])+skalars4[4]*(G1[4]-G2[4])+skalars4[5]*(-G1[5]-G2[5])+skalars4[6]*(G1[6]-G2[6])+skalars4[7]*(-G1[7]-G2[7])
Y8 = G1[4] + G2[4]

Y10 = G1[5] - G2[5]

Y12 = G1[6] + G2[6]

Y14 = G1[7] - G2[7]

print("Y(0)=",Y0)
print("Y(1)=",Y1)
print("Y(2)=",Y2)
print("Y(3)=",Y3)
print("Y(4)=",Y4)
print("Y(5)=",Y5)
print("Y(6)=",Y6)
print("Y(7)=",Y7)
print("Y(8)=",Y8)
print("Y(10)=",Y10)
print("Y(12)=",Y12)
print("Y(14)=",Y14)
