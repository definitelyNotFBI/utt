# Todo
- Use KVBC replica

# Eval Plan (Essential, Important, Nice to have) 
TODO: Put it on Overleaf (eval.tex)
- Micro benchmarks on the target machine (E)
- Throughput vs. Latency for different batch sizes (For regular payments) (E)
- Throughput vs. n (For regular) (E)
- Latency vs. n (For regular) (E)
- Throughput vs. Latency for different batch sizes (For quickpay payments) (I)
(Fix batch size from the above experiment; say that we use the parameters from this to limit latency)
- Throughput vs. n (For quickpay) (I)
- Latency vs. n (For quickpay) (I)
- Throughput vs. # Shards (I)

- Mix Tx Throughput [Mint, QuickPay, (Lightning), Payment] (N)
- Throughput vs. n (For Budget) (N)
- Latency vs. n (For Budget) (N)
- Throughput vs. # Shards (For Budget) (N)

# Poster Session
BFT Layer integration

- Overview of UTT
- Pre-Execution
- BFT vs. QuickPay (We can even do anonymous payments; The Why factor?) + Animations
- Sharding (Without 2-Phase commits) + Animations
- Demo

# Parameters to play
- SELF_ADJUSTED
- concurrencyLevel (from 4 -> TEST) Also update batchingFactorCoefficient
-