#ifndef CRYPTOPP_TIGER_H
#define CRYPTOPP_TIGER_H

#include "config.h"
#include "iterhash.h"

NAMESPACE_BEGIN(CryptoPP)

/// <a href="http://www.cryptolounge.org/wiki/Tiger">Tiger</a>
class CRYPTOPP_DLL Tiger : public IteratedHashWithStaticTransform<word64, LittleEndian, 64, 24, Tiger>
{
public:
	static void InitState(HashWordType *state);
	static void Transform(word64 *digest, const word64 *data);
	void TruncatedFinal(byte *hash, size_t size);
	//static const char * StaticAlgorithmName() {return "Tiger";}
	static void StaticAlgorithmName(char* Name) {strcat(Name, "Tiger");}

protected:
	static const word64 table[4*256+3];
};

NAMESPACE_END

#endif
