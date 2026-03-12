import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("gen_logs/generation_summary.csv")

plt.figure(figsize=(10, 6))
plt.plot(df["generation"], df["avgFitness"], label="Average Fitness")
plt.plot(df["generation"], df["bestFitness"], label="Best Fitness")
plt.xlabel("Generation")
plt.ylabel("Fitness")
plt.title("Evolution of Fitness Over Generations")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()

# df = pd.read_csv("gen_logs/generation_5.csv")

# plt.figure(figsize=(8, 6))
# plt.scatter(df["gSize"], df["fitness"])
# plt.xlabel("Genome Size")
# plt.ylabel("Fitness")
# plt.title("Generation 10: Size vs Fitness")
# plt.grid(True)
# plt.tight_layout()
# plt.show()