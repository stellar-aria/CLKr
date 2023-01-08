# standard 1-D parametric gaussian function (for LED breathing)
# (see https://en.wikipedia.org/wiki/Gaussian_function)

import math

smoothness_pts = 500  # larger=slower change in brightness
gamma = 0.14          # affects the width of the peak
beta = 0            # shifts the gaussian to be symmetric
alpha = 255.0         # maximum amplitude

float_gauss = [alpha * (math.exp(-(math.pow(((x / smoothness_pts) - beta) / gamma, 2.0)) / 2.0))
               for x in range(smoothness_pts)]
gauss = [math.ceil(x) for x in float_gauss]
print(gauss)
print(len(gauss))
