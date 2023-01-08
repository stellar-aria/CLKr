from numpy import interp, linspace
# This LUT smooths between 21250 (our fastest legacy clock speed, dependent on the prescaler)
# and 6250000 (inclusive!), which is not exactly but close enough to the 2.44s of the original (2.5s now)

size = 256
max_val = int(6125000 / 2)  # divide by two as clock edge is 1/2 of period
min_val = int(20400 / 2)

# reversed because our lowest value is actually our fastest timer interval (and corresponds to the high value of the pot)
spread = [round(x) for x in linspace(max_val, min_val, size)]  # linear scaler
print(spread)
print(len(spread))

# this is a logarithmic scaler
# see https://electronics.stackexchange.com/a/341052 for details
y_m = 0.9  # midpoint
b = (1/y_m - 1)**2
a = 1 / (b - 1)
log_vals = [a*(b**x) - a for x in linspace(0, 1, size)] # 0 to 1 mapping
log_vals = [round(interp(x, [min(log_vals), max(log_vals)], [max_val, min_val])) for x in log_vals]
print(log_vals)
print(len(log_vals))