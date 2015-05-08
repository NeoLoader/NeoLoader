#ifndef CRYPTOPP_WHIRLPOOL_H
#define CRYPTOPP_WHIRLPOOL_H

#include "config.h"
#include "iterhash.h"

NAMESPACE_BEGIN(CryptoPP)

//! <a href="http://www.cryptolounge.org/wiki/Whirlpool">Whirlpool</a>
class CRYPTOPP_DLL Whirlpool : public IteratedHashWithStaticTransform<word64, BigEndian, 64, 64, Whirlpool>
{
public:
	static void InitState(HashWordType *state);
	static void Transform(word64 *digest, const word64 *data);
	void TruncatedFinal(byte *hash, size_t size);
	//static const char * StaticAlgorithmName() {return "Whirlpool";}
	static void StaticAlgorithmName(char* Name) {strcat(Name, "Whirlpool");}
};

NAMESPACE_END

#endif
