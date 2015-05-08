#pragma once
#include "../Framework/Cryptography/AbstractKey.h"

class KeygenRSAPrimeSelector : public CryptoPP::PrimeSelector
{
public:
	KeygenRSAPrimeSelector(const CryptoPP::Integer &e) : m_e(e) {}
	bool IsAcceptable(const CryptoPP::Integer &candidate) const {return CryptoPP::RelativelyPrime(m_e, candidate-CryptoPP::Integer::One());}
	CryptoPP::Integer m_e;
};

__inline size_t MakeKeyRSA(byte* pKey, size_t uSize, int modulusSize = 2048)
{
	CryptoPP::AutoSeededRandomPool rng;
	ASSERT(modulusSize >= 16);
	CryptoPP::Integer e = CryptoPP::Integer(17);
	ASSERT(e >= 3 && !e.IsEven());
		
	KeygenRSAPrimeSelector selector(e);
	CryptoPP::AlgorithmParameters primeParam = CryptoPP::MakeParametersForTwoPrimesOfEqualSize(modulusSize)
		("PointerToPrimeSelector", selector.GetSelectorPointer());
	CryptoPP::Integer p; p.GenerateRandom(rng, primeParam);
	CryptoPP::Integer q; q.GenerateRandom(rng, primeParam);
	CryptoPP::Integer d = e.InverseMod(LCM(p-1, q-1));
	ASSERT(d.IsPositive());

	CryptoPP::InvertibleRSAFunction privke;
	privke.SetModulus(/*n =*/ p * q); 
	privke.SetPublicExponent(e);
	privke.SetPrime1(p); 
	privke.SetPrime2(q);
	privke.SetPrivateExponent(d); 
	privke.SetModPrime1PrivateExponent(/*dp =*/ d % (p-1)); 
	privke.SetModPrime2PrivateExponent(/*dq =*/ d % (q-1)); 
	privke.SetMultiplicativeInverseOfPrime2ModPrime1(/*u =*/ q.InverseMod(p));

	CryptoPP::ArraySink PrivKeySink(pKey, uSize);
	privke.DEREncode(PrivKeySink);
	return PrivKeySink.TotalPutLength();
}