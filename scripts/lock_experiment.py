import sys


def generate_lock_experiment(TL2):
    TL1 = 5000 - TL2
    interleavePos = 2500

    with open("traces/human_readable_traces/lock_experiment.txt", "w") as outputFile:
        # Thread 1 has TL1 pairs of acq-rel and one write on x at the end
        # Thread 2 has TL2 pairs of acq-rel and one write on x in between each acq-rel pairs
        # TL2 interleaves with TL1 at position interleavePos

        for _ in range(interleavePos):
            outputFile.write("Acq 1 l_0 0\n")
            outputFile.write("Rel 1 l_0 0\n")

        for _ in range(TL2):
            outputFile.write("Acq 2 l_0 0\n")
            outputFile.write("Write 2 X_0 2\n")
            outputFile.write("Rel 2 l_0 0\n")

        for _ in range(interleavePos, TL1):
            outputFile.write("Acq 1 l_0 0\n")
            outputFile.write("Rel 1 l_0 0\n")
        outputFile.write("Write 1 X_0 1\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python lock_experiment.py <TL2>")
        sys.exit(1)

    TL2 = int(sys.argv[1])
    generate_lock_experiment(TL2)
