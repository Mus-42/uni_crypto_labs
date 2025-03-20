import matplotlib.pyplot as plt

counts = [0] * 26
while True:
    try:
        s = input().lower()
        for ch in s:
            if ch.isalpha():
                counts[ord(ch) - ord('a')] += 1
    except EOFError:
        break

labels = [chr(ord('a') + i) for i in range(26)]
plt.hist(labels, bins=26, weights=counts)
plt.show()
