#include "GlobalHeader.h"
#include "Crypto.h"
#include "../../Framework/Cryptography/AbstractKey.h"
#include "../../Framework/Cryptography/KeyExchange.h"
#include "../../Framework/Cryptography/SymmetricKey.h"
#include "../../Framework/Strings.h"
#include "../../Framework/Scope.h"

// 1024 bit RSA
QByteArray Sec_DH_1024 = QByteArray::fromHex(
"3082010a02818100ad087600d3df7a537aa87f66004fbb6444576879d43e3f55"
"25a7cf65a0fd351d432c933957778f450cf845a6450ba0c31f93f8bbf8f4c77d"
"f6e13c44c1277498344b6e367b3bda4deb3a81fa744cf46963e18cab7659f344"
"17c29a17acb79e36123e0b2446b3f46c5d1f5038a5f5891e7c03e26c5895f0ee"
"61504d90a4007bf302818056843b0069efbd29bd543fb30027ddb2222bb43cea"
"1f1faa92d3e7b2d07e9a8ea196499cabbbc7a2867c22d32285d0618fc9fc5dfc"
"7a63befb709e226093ba4c1a25b71b3d9ded26f59d40fd3a267a34b1f0c655bb"
"2cf9a20be14d0bd65bcf1b091f05922359fa362e8fa81c52fac48f3e01f1362c"
"4af87730a826c852003df9020103");

// 2048 bit RSA
QByteArray Sec_DH_2048 = QByteArray::fromHex(
"3082020c0282010100be3a8b0692139ee335bf9bab0c4972adaed1b89b0d3392"
"9f9dd517e853b0a5cf801de3bb09d218e8295c013dfa69fa7a7b6689a6e89666"
"4e54cce3dbd513e53cc7baf8582107270e9d8b2b423b5a0abcfe5845ff34350a"
"267c11ed76da523fafdf37a69bcfe78dcd1d5e2fdae26e72b89acb231326e5fe"
"c106eb42ccde4b6fe6b85777346a5dfe82f202355bc24617f1c33060a57c04dd"
"16826a719e95161eb86bc78679bb65369059c6a461e657f1d69ff4ccd0b94a54"
"5f28d8babac692b6fc6c64f39da70abdda62f487137914311a11f70a7af43d0b"
"30d127bd2160f4053a8e8cd7348e50ebf5bec909675dbad58315dafb4d5ddd8a"
"bb0b0359e452182ff3028201005f1d45834909cf719adfcdd58624b956d768dc"
"4d8699c94fceea8bf429d852e7c00ef1dd84e90c7414ae009efd34fd3d3db344"
"d3744b33272a6671edea89f29e63dd7c2c108393874ec595a11dad055e7f2c22"
"ff9a1a85133e08f6bb6d291fd7ef9bd34de7f3c6e68eaf17ed7137395c4d6591"
"899372ff608375a1666f25b7f35c2bbb9a352eff4179011aade1230bf8e19830"
"52be026e8b413538cf4a8b0f5c35e3c33cddb29b482ce35230f32bf8eb4ffa66"
"685ca52a2f946c5d5d63495b7e363279ced3855eed317a4389bc8a188d08fb85"
"3d7a1e85986893de90b07a029d47466b9a472875fadf6484b3aedd6ac18aed7d"
"a6aeeec55d8581acf2290c17f9020103");

// 3072 bit would be 128 AES 
QByteArray Sec_DH_3072 = QByteArray::fromHex(
"3082040c02820201009fc608fdc60d2512697457b69b426c6b79a73f8e4dfe70"
"897e294b3d5d959f3a53bc6886a09fa69cbbfb85376dc938f5e265f43d59c417"
"0e7e59f4471fe2e1af299abbbb419309fc6e21a2bc70c96ef31836a21b8b1fa8"
"404baed94a6cc02ce2d33e0f6d72871004b764cc2a602352115d0f6f3acec504"
"9169be5c25c56ea60f1dfd802efd9e2f74968459be3babb7849b2494634496b1"
"17f3dda024dc431a0004a1acb93e4680d5d4b9f11963b530acf4e2ea80334826"
"7d7efc3744ebdd106e27f3ff52321710ba93ff09386d03a98ba6bdbd3f585bde"
"b851ecaea99b26f2a78b7eebfacf3c21c23c35046f10887bb5cfb22cb0d3beb5"
"fa13dc30018fdd7004c04715ada86359e1e15634c299fbb8e3673b29c44b4658"
"f81a21330c24ab82fd188aacfd99736c9e22a11f4e3945f77009a523f62c5265"
"704a8c3a4544abc8e8d75044c7be69b19866dc411ad170021f9f4310a91a9453"
"b277f62bc77d6eccc249701f0c8786363de8dc6992f05c63992bea6b284fd836"
"a72277c3b37d8ba22215f171776631fb6ee2b7b9e85d1963a8f0f48e3911bc06"
"97a1380f0b2cd17317487b526444e1b3da6fc1681240f24445401c2140483e7a"
"5dba11eff288fd187bb972a6aed902e953e77ae729c694b966ea532d13cd0e2e"
"de4d0c4f17392177d8897c013038a514870e42bf4c8a9cc3f082ebd2aa2b683e"
"e4ace5d4ed0daafc87028202004fe3047ee306928934ba2bdb4da13635bcd39f"
"c726ff3844bf14a59eaecacf9d29de3443504fd34e5dfdc29bb6e49c7af132fa"
"1eace20b873f2cfa238ff170d794cd5ddda0c984fe3710d15e3864b7798c1b51"
"0dc58fd42025d76ca536601671699f07b6b94388025bb266153011a908ae87b7"
"9d67628248b4df2e12e2b753078efec0177ecf17ba4b422cdf1dd5dbc24d924a"
"31a24b588bf9eed0126e218d000250d65c9f23406aea5cf88cb1da98567a7175"
"4019a4133ebf7e1ba275ee883713f9ffa9190b885d49ff849c3681d4c5d35ede"
"9fac2def5c28f65754cd937953c5bf75fd679e10e11e1a823788443ddae7d916"
"5869df5afd09ee1800c7eeb80260238ad6d431acf0f0ab1a614cfddc71b39d94"
"e225a32c7c0d1099861255c17e8c45567eccb9b64f11508fa71ca2fbb804d291"
"fb162932b825461d22a255e4746ba82263df34d8cc336e208d68b8010fcfa188"
"548d4a29d93bfb15e3beb7666124b80f8643c31b1ef46e34c9782e31cc95f535"
"9427ec1b53913be1d9bec5d1110af8b8bbb318fdb7715bdcf42e8cb1d4787a47"
"1c88de034bd09c07859668b98ba43da9322270d9ed37e0b40920792222a00e10"
"a0241f3d2edd08f7f9447e8c3ddcb953576c8174a9f3bd7394e34a5cb3752996"
"89e687176f2686278b9c90bbec44be00981c528a4387215fa6454e61f84175e9"
"5515b41f725672ea7686d57e43020103");

UINT PrepSC(UINT eAlgorithm)
{
	UINT eSymCipher = eAlgorithm & CAbstractKey::eSymCipher;
	if(eSymCipher == 0)
		eSymCipher = CAbstractKey::eAES;
	UINT eSymMode = eAlgorithm & CAbstractKey::eSymMode;
	if(eSymMode == 0)
		eSymMode = CAbstractKey::eCFB;
	UINT eSymHash = eAlgorithm & CAbstractKey::eHashFunkt;
	if(eSymHash == 0)
		eSymHash = CAbstractKey::eSHA256;

	return eSymCipher | eSymMode | eSymHash;
}

UINT ParseSC(const string& sSC)
{
	vector<string> SC = SplitStr(sSC, "/");
	UINT eSymCipher = CAbstractKey::Str2Algorithm(SC.at(0)) & CAbstractKey::eSymCipher;
	if(eSymCipher == 0)
		throw "InvalidParam: CS";
	UINT eSymMode = 0;
	if(SC.size() >= 2)
	{
		eSymMode = CAbstractKey::Str2Algorithm(SC.at(1)) & CAbstractKey::eSymMode;
		if(eSymMode == 0)
			throw "InvalidParam: CS";
	}
	UINT eSymHash = 0;
	if(SC.size() >= 3)
	{
		eSymHash = CAbstractKey::Str2Algorithm(SC.at(2)) & CAbstractKey::eHashFunkt;
		if(eSymHash == CAbstractKey::eNone)
			throw "InvalidParam: CS";
	}

	return eSymCipher | eSymMode | eSymHash;
}

CSymmetricKey* NewSymmetricKey(UINT eAlgorithm, CAbstractKey* pKey, CAbstractKey* pInIV, CAbstractKey* pOutIV)
{
	CScoped<CSymmetricKey> pCryptoKey = new CSymmetricKey(eAlgorithm & (CAbstractKey::eSymCipher | CAbstractKey::eSymMode | CAbstractKey::eHashFunkt));
	bool bOK = true; // if one fails all fail!
	bOK &= pCryptoKey->SetKey(pKey->GetKey(), pKey->GetSize());
	bOK &= pCryptoKey->SetupEncryption(pOutIV->GetKey(), pOutIV->GetSize());
	bOK &= pCryptoKey->SetupDecryption(pInIV->GetKey(), pInIV->GetSize());
	if(!bOK)
		throw "CryptoError";
	return pCryptoKey.Detache();
}

UINT PrepKA(UINT eAlgorithm, string& Param)
{
	UINT eKeyExchange = eAlgorithm & CAbstractKey::eKeyExchange;
	if(eKeyExchange == 0)
		eKeyExchange = CAbstractKey::eECDH;
	UINT eAsymCipher = eAlgorithm & CAbstractKey::eAsymCipher;	
	if(eKeyExchange == CAbstractKey::eECDH && eAsymCipher == 0)
		eAsymCipher = CAbstractKey::eECP;
	UINT eAsymHash = eAlgorithm & CAbstractKey::eHashFunkt;
	if(eAsymHash == 0)
		eAsymHash = CAbstractKey::eSHA256;

	if(Param.empty())
	{
		if(eKeyExchange == CAbstractKey::eECDH)
		{
			switch(eAsymCipher)
			{
				case CAbstractKey::eECP:	Param = "brainpoolP256r1";  break;	// secp256r1 - dont trust NIST (NSA) curves, use brainpool
				case CAbstractKey::eEC2N:	Param = "sect283k1"; break;			// this ine is NIST we dont have default brainpool curves for that
			}
		}
		else if(eKeyExchange ==  CAbstractKey::eDH)
			Param = "2048";
	}

	return eKeyExchange | eAsymCipher | eAsymHash;
}

UINT ParseKA(const string& sKA, string& Param)
{
	pair<string,string> KA_P = Split2(sKA, "-");
	Param = KA_P.second;

	vector<string> KA = SplitStr(KA_P.first, "/");
	UINT eKeyExchange = CAbstractKey::Str2Algorithm(KA.at(0)) & CAbstractKey::eKeyExchange;
	UINT eAsymCipher = 0;
	if(KA.size() >= 2)
		eAsymCipher = CAbstractKey::Str2Algorithm(KA.at(1)) & CAbstractKey::eAsymCipher;
	UINT eAsymHash = 0;
	if(KA.size() >= 3)
		eAsymHash = CAbstractKey::Str2Algorithm(KA.at(2)) & CAbstractKey::eHashFunkt;

	return eKeyExchange | eAsymCipher | eAsymHash;
}

CKeyExchange* NewKeyExchange(UINT eAlgorithm, const string& Param)
{
	CScoped<CKeyExchange> pKeyExchange;
	if(eAlgorithm & CAbstractKey::eECDH)
	{
		if((eAlgorithm & CAbstractKey::eAsymCipher) == 0)
			throw "InvalidParam: KA";

		pKeyExchange = new CKeyExchange(eAlgorithm, Param.c_str());
		if(pKeyExchange->GetAlgorithm() == CAbstractKey::eUndefined)
			throw "UnknownCurve";
	}
	else if(eAlgorithm & CAbstractKey::eDH)
	{
		if(Param == "1024")
			pKeyExchange = new CKeyExchange(CAbstractKey::eDH, (byte*)Sec_DH_1024.data(), Sec_DH_1024.size());
		else if(Param == "2048")
			pKeyExchange = new CKeyExchange(CAbstractKey::eDH, (byte*)Sec_DH_2048.data(), Sec_DH_2048.size());
		else if(Param == "3072")
			pKeyExchange = new CKeyExchange(CAbstractKey::eDH, (byte*)Sec_DH_3072.data(), Sec_DH_3072.size());
		else
			throw "UnknownPrime";
	}
	else
		throw "NotSupported";

	return pKeyExchange.Detache();
}