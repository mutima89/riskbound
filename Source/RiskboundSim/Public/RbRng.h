// Deterministic random number generation for reproducible scenarios.
// xoshiro256** seeded through SplitMix64. Same seed -> same sequence on every platform.
#pragma once

#include <cmath>
#include <cstdint>

namespace rb
{
class Rng
{
public:
	explicit Rng(uint64_t Seed = 0x5EED5EEDULL) { Reseed(Seed); }

	void Reseed(uint64_t Seed)
	{
		uint64_t X = Seed;
		for (uint64_t& Word : State)
		{
			Word = SplitMix64(X);
		}
		bHasSpareNormal = false;
	}

	uint64_t NextU64()
	{
		const uint64_t Result = Rotl(State[1] * 5, 7) * 9;
		const uint64_t T = State[1] << 17;
		State[2] ^= State[0];
		State[3] ^= State[1];
		State[1] ^= State[2];
		State[0] ^= State[3];
		State[2] ^= T;
		State[3] = Rotl(State[3], 45);
		return Result;
	}

	// Uniform in [0, 1).
	double Uniform() { return static_cast<double>(NextU64() >> 11) * (1.0 / 9007199254740992.0); }

	// Uniform integer in [Lo, Hi] inclusive.
	int UniformInt(int Lo, int Hi)
	{
		if (Hi <= Lo)
		{
			return Lo;
		}
		const uint64_t Span = static_cast<uint64_t>(Hi - Lo) + 1;
		return Lo + static_cast<int>(NextU64() % Span);
	}

	bool Bernoulli(double P) { return Uniform() < P; }

	// Standard normal via Box-Muller (deterministic, caches the spare value).
	double Normal()
	{
		if (bHasSpareNormal)
		{
			bHasSpareNormal = false;
			return SpareNormal;
		}
		double U1 = Uniform();
		while (U1 <= 1e-300)
		{
			U1 = Uniform();
		}
		const double U2 = Uniform();
		const double R = std::sqrt(-2.0 * std::log(U1));
		const double Theta = 6.283185307179586 * U2;
		SpareNormal = R * std::sin(Theta);
		bHasSpareNormal = true;
		return R * std::cos(Theta);
	}

	// Lognormal with parameters of the underlying normal distribution.
	double LogNormal(double Mu, double Sigma) { return std::exp(Mu + Sigma * Normal()); }

	// Poisson count. Knuth's method for small lambda, normal approximation above 30.
	int Poisson(double Lambda)
	{
		if (Lambda <= 0.0)
		{
			return 0;
		}
		if (Lambda > 30.0)
		{
			const double V = Lambda + std::sqrt(Lambda) * Normal();
			return V < 0.0 ? 0 : static_cast<int>(V + 0.5);
		}
		const double L = std::exp(-Lambda);
		int K = 0;
		double P = 1.0;
		do
		{
			++K;
			P *= Uniform();
		} while (P > L);
		return K - 1;
	}

private:
	static uint64_t Rotl(uint64_t X, int K) { return (X << K) | (X >> (64 - K)); }
	static uint64_t SplitMix64(uint64_t& X)
	{
		uint64_t Z = (X += 0x9E3779B97F4A7C15ULL);
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBULL;
		return Z ^ (Z >> 31);
	}

	uint64_t State[4] = {};
	double SpareNormal = 0.0;
	bool bHasSpareNormal = false;
};
} // namespace rb
