#pragma once

#include "PublicKey.h"
#include "PrivateKey.h"

#define PP_PATCH

namespace CryptoPP
{

//! Elliptic Curve Integrated Encryption Scheme, AKA <a href="http://www.weidai.com/scan-mirror/ca.html#ECIES">ECIES</a>
/*! Default to (NoCofactorMultiplication and DHAES_MODE = false) for compatibilty with SEC1 and Crypto++ 4.2.
	The combination of (IncompatibleCofactorMultiplication and DHAES_MODE = true) is recommended for best
	efficiency and security. */
template <class EC, class H, class COFACTOR_OPTION = NoCofactorMultiplication, bool DHAES_MODE = false>
struct ECIES_
	: public DL_ES<
		DL_Keys_EC<EC>,
		DL_KeyAgreementAlgorithm_DH<typename EC::Point, COFACTOR_OPTION>,
		DL_KeyDerivationAlgorithm_P1363<typename EC::Point, DHAES_MODE, P1363_KDF2<H> >,
		DL_EncryptionAlgorithm_Xor<HMAC<H>, DHAES_MODE>,
		ECIES_<EC, H> >
{
#ifdef PP_PATCH
	static void CRYPTOPP_API StaticAlgorithmName(char* Name) { strcat(Name, "ECIES"); }
#else
	static std::string CRYPTOPP_API StaticAlgorithmName() {return "ECIES";} // TODO: fix this after name is standardized
#endif
};

//! LUC-IES
template <class H, class COFACTOR_OPTION = NoCofactorMultiplication, bool DHAES_MODE = true>
struct LUC_IES_
	: public DL_ES<
		DL_CryptoKeys_LUC,
		DL_KeyAgreementAlgorithm_DH<Integer, COFACTOR_OPTION>,
		DL_KeyDerivationAlgorithm_P1363<Integer, DHAES_MODE, P1363_KDF2<H> >,
		DL_EncryptionAlgorithm_Xor<HMAC<H>, DHAES_MODE>,
		LUC_IES_<H> >
{
#ifdef PP_PATCH
	static void StaticAlgorithmName(char* Name) { strcat(Name, "LUC-IES"); }
#else
	static std::string StaticAlgorithmName() {return "LUC-IES";}	// non-standard name
#endif
};

//! Discrete Log Integrated Encryption Scheme, AKA <a href="http://www.weidai.com/scan-mirror/ca.html#DLIES">DLIES</a>
template <class H, class COFACTOR_OPTION = NoCofactorMultiplication, bool DHAES_MODE = true>
struct DLIES_
	: public DL_ES<
		DL_CryptoKeys_GFP,
		DL_KeyAgreementAlgorithm_DH<Integer, COFACTOR_OPTION>,
		DL_KeyDerivationAlgorithm_P1363<Integer, DHAES_MODE, P1363_KDF2<H> >,
		DL_EncryptionAlgorithm_Xor<HMAC<H>, DHAES_MODE>,
		DLIES_<H> >
{
#ifdef PP_PATCH
	static void StaticAlgorithmName(char* Name) { strcat(Name, "DLIES"); }
#else
	static std::string CRYPTOPP_API StaticAlgorithmName() {return "DLIES";}	// TODO: fix this after name is standardized
#endif
};

}

#define GET_H(KC, OP) \
PK_##OP* KC::Get##OP##L1(UINT eAlgorithm)\
{\
	switch(eAlgorithm & eHashFunkt)\
	{\
		case 0: \
		case eSHA1:			return Get##OP##L2<SHA1>(eAlgorithm);\
		case eSHA224:		return Get##OP##L2<SHA224>(eAlgorithm);\
		case eSHA256:		return Get##OP##L2<SHA256>(eAlgorithm);\
		case eSHA384:		return Get##OP##L2<SHA384>(eAlgorithm);\
		case eSHA512:		return Get##OP##L2<SHA512>(eAlgorithm);\
		case eTiger:		return Get##OP##L2<Tiger>(eAlgorithm);\
		case eWhirlpool:	return Get##OP##L2<Whirlpool>(eAlgorithm);\
		case eRIPEMD128:	return Get##OP##L2<RIPEMD128>(eAlgorithm);\
		case eRIPEMD160:	return Get##OP##L2<RIPEMD160>(eAlgorithm);\
		case eRIPEMD256:	return Get##OP##L2<RIPEMD256>(eAlgorithm);\
		case eRIPEMD320:	return Get##OP##L2<RIPEMD320>(eAlgorithm);\
		default: throw 0;\
	}\
}\

#define GET_ES(KC, OP) \
template<typename H> \
PK_##OP* KC::Get##OP##L2(UINT eAlgorithm) \
{\
	switch(m_eAlgorithm & eAsymCipher)\
	{\
		case eRSA:\
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eOAEP: return new typename RSAES<OAEP<H> >::OP();\
				case ePKCS: return new typename RSAES<PKCS1v15>::OP();\
				default: throw 0;\
			}\
		case eLUCX:\
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eOAEP: return new typename LUCES<OAEP<H> >::OP();\
				case ePKCS: return new typename LUCES<PKCS1v15>::OP();\
				default: throw 0;\
			}\
		case eLUC:\
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eIES:	return new typename LUC_IES_<H>::OP();\
				default: throw 0;\
			}\
		case eRabin:\
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eOAEP:	return new typename RabinES<OAEP<H> >::OP();\
				case ePKCS:	return new typename RabinES<PKCS1v15>::OP();\
				default: throw 0;\
			}\
		case eDL:\
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eIES:	return new typename DLIES_<H>::OP();\
				default: throw 0;\
			}\
		case eECP:\
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eIES:	return new typename ECIES_<ECP, H>::OP();\
				default: throw 0;\
			}\
		case eEC2N: \
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eIES:	return new typename ECIES_<EC2N, H>::OP();\
				default: throw 0;\
			}\
		default: throw 0;\
	}\
}\

#define GET_SS(KC, OP) \
template<typename H> \
PK_##OP* KC::Get##OP##L2(UINT eAlgorithm) \
{\
	switch(eAlgorithm & eAsymCipher)\
	{\
		case eRSA:\
			switch(eAlgorithm & eAsymMode) \
			{\
				case ePSSR:	return new typename RSASS<PSSR, H>::OP();\
				case ePSS:	return new typename RSASS<PSS, H>::OP();\
				default: throw 0;\
			}\
		case eLUCX:\
		{\
			switch(eAlgorithm & eAsymMode) \
			{\
				case ePSSR:	return new typename LUCSS<PSSR, H>::OP();\
				case ePSS:	return new typename LUCSS<PSS, H>::OP();\
				default: throw 0;\
			}\
			break;\
		}\
		case eLUC:\
			switch(eAlgorithm & eAsymMode)\
			{\
				case 0:\
				case eDSA:	return new typename LUC_HMP<H>::OP();\
				default: throw 0;\
			}\
		case eRabin:\
			switch(eAlgorithm & eAsymMode) \
			{\
				case 0: \
				case ePSSR:	return new typename RabinSS<PSSR, H>::OP();\
				case ePSS:	return new typename RabinSS<PSS, H>::OP();\
				default: throw 0;\
			}\
			break;\
		case eDL:\
			switch(eAlgorithm & eAsymMode) \
			{\
				case 0: \
				case eDSA:	return new typename GDSA<H>::OP();\
				case eNR:	return new typename NR<H>::OP();\
				default: throw 0;\
			}\
			break;\
		case eECP:\
			switch(eAlgorithm & eAsymMode) \
			{\
				case 0: \
				case eDSA:	return new typename ECDSA<ECP, H>::OP();\
				case eNR:	return new typename ECNR<ECP, H>::OP();\
				default: throw 0;\
			}\
			break;\
		case eEC2N:\
			switch(eAlgorithm & eAsymMode) \
			{\
				case 0: \
				case eDSA:	return new typename ECDSA<EC2N, H>::OP();\
				case eNR:	return new typename ECNR<EC2N, H>::OP();\
				default: throw 0;\
			}\
			break;\
		default: throw 0;\
	}\
}\

// Note: PKCS1v15 supports only selected hashes
#define GET_HP(KC, OP) \
PK_##OP* KC::Get##OP##PL1(UINT eAlgorithm)\
{\
	switch(eAlgorithm & eHashFunkt)\
	{\
		case 0: \
		case eSHA1:			return Get##OP##PL2<SHA1>(eAlgorithm);\
		case eSHA224:		return Get##OP##PL2<SHA224>(eAlgorithm);\
		case eSHA256:		return Get##OP##PL2<SHA256>(eAlgorithm);\
		case eSHA384:		return Get##OP##PL2<SHA384>(eAlgorithm);\
		case eSHA512:		return Get##OP##PL2<SHA512>(eAlgorithm);\
		case eTiger:		return Get##OP##PL2<Tiger>(eAlgorithm);\
		case eRIPEMD160:	return Get##OP##PL2<RIPEMD160>(eAlgorithm);\
		default: throw 0;\
	}\
}\

#define GET_SSP(KC, OP) \
template<typename H> \
PK_##OP* KC::Get##OP##PL2(UINT eAlgorithm) \
{\
	switch(eAlgorithm & eAsymCipher)\
	{\
		case eRSA: return	new typename RSASS<PKCS1v15, H>::OP();\
		case eLUCX: return	new typename LUCSS<PKCS1v15, H>::OP();\
		case eRabin: return new typename RabinSS<PKCS1v15, H>::OP();\
		default: throw 0;\
	}\
}\

