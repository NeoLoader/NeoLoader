#include "GlobalHeader.h"
#include "AsymmetricKey.h"
#include "HashFunction.h"

using namespace CryptoPP;

CPrivateKey::CPrivateKey(UINT eAlgorithm)
{
	m_eAlgorithm = eAlgorithm;
}

bool CPrivateKey::SetupKey(byte* pKey, size_t uSize)
{
	if(m_eAlgorithm == eUndefined)
	{
		try
		{
			ArraySource bt(pKey,uSize,true,0);

			BERSequenceDecoder privateKeyInfo(bt);
				word32 version;
				BERDecodeUnsigned<word32>(privateKeyInfo, version, INTEGER, 0, 0);	// check version

				BERSequenceDecoder algorithm(privateKeyInfo);
				OID oid(algorithm);
				algorithm.SkipAll();
				algorithm.MessageEnd();

			privateKeyInfo.SkipAll();
			privateKeyInfo.MessageEnd();

			if(InvertibleRSAFunction().GetAlgorithmID() == oid)
				m_eAlgorithm  = eRSA;
			else if(DL_CryptoKeys_LUC::PrivateKey().GetAlgorithmID() == oid)
				m_eAlgorithm  = eLUC;
			else if(DL_CryptoKeys_GFP::PrivateKey().GetAlgorithmID() == oid)
				m_eAlgorithm  = eDL;
			else if(DL_PrivateKey_EC<ECP>().GetAlgorithmID() == oid)
				m_eAlgorithm  = eECP;
			else if(DL_PrivateKey_EC<EC2N>().GetAlgorithmID() == oid)
				m_eAlgorithm  = eEC2N;
			else
				return false;
		}
		catch(...)
		{
			ASSERT(0);
			return false;
		}
	}

	return CAbstractKey::SetupKey(pKey, uSize);
}

GET_H(CPrivateKey, Signer)
GET_SS(CPrivateKey, Signer)
GET_HP(CPrivateKey, Signer)
GET_SSP(CPrivateKey, Signer)

PK_Signer* CPrivateKey::GetSigner(UINT eAlgorithm)
{
	if((eAlgorithm & eAsymMode) == ePKCS || ((eAlgorithm & eAsymMode) == 0  && ((eAlgorithm & eAsymCipher) == eRSA || (eAlgorithm & eAsymCipher) == eLUCX)))
		return GetSignerPL1(eAlgorithm);
	return GetSignerL1(eAlgorithm);
}

bool CPrivateKey::Sign(byte* pPayload, size_t uPayloadSize, byte*& pSignature, size_t& uSignatureSize, UINT eAlgorithm)
{
	if((eAlgorithm & eAsymCipher) == 0)
		eAlgorithm |= (m_eAlgorithm & eAsymCipher);
	else if((eAlgorithm & eAsymCipher) != m_eAlgorithm)
	{
		ASSERT(0); // key incompatible!
		return false;
	}

	PK_Signer* Signer = NULL;
	try
	{
		Signer = GetSigner(eAlgorithm);
		ArraySource KeySource(GetKey(),GetSize(),true,0);
		Signer->BERDecode(KeySource);

		uSignatureSize = Signer->SignatureLength();
		ASSERT(pSignature == NULL);
		pSignature = new byte[uSignatureSize];

		AutoSeededRandomPool rng;
		Signer->SignMessage(rng, pPayload, uPayloadSize, pSignature);

		delete Signer;
		return true;
	}
	catch(...)
	{
		ASSERT(0);
		delete Signer;
		return false;
	}
}

bool CPrivateKey::Sign(CBuffer* pPayload, CBuffer* pSignature, UINT eAlgorithm)
{
	byte* pSignatureBuff = NULL;
	size_t uSignatureSize = 0;
	if(!Sign(pPayload->GetBuffer(), pPayload->GetSize(), pSignatureBuff, uSignatureSize, eAlgorithm))
		return false;
	pSignature->SetBuffer(pSignatureBuff, uSignatureSize);
	return true;
}

GET_H(CPrivateKey, Decryptor)
GET_ES(CPrivateKey, Decryptor)

PK_Decryptor* CPrivateKey::GetDecryptor(UINT eAlgorithm)
{
	return GetDecryptorL1(eAlgorithm);
}

bool CPrivateKey::Decrypt(byte* pCiphertext, size_t uCiphertextSize, byte*& pPlaintext, size_t& uPlaintextSize, UINT eAlgorithm)
{
	if((eAlgorithm & eAsymCipher) == 0)
		eAlgorithm |= (m_eAlgorithm & eAsymCipher);
	else if((eAlgorithm & eAsymCipher) != m_eAlgorithm)
	{
		ASSERT(0); // key incompatible!
		return false;
	}

	PK_Decryptor* Decryptor = NULL;
	try
	{
		Decryptor = GetDecryptor(eAlgorithm);
		ArraySource KeySource(GetKey(),GetSize(),true,0);
		Decryptor->BERDecode(KeySource);

		uPlaintextSize = Decryptor->MaxPlaintextLength(uCiphertextSize);
		ASSERT(uPlaintextSize > 0); // invalid length
		if(uPlaintextSize == 0)
			return false;
		ASSERT(pPlaintext == NULL);
		pPlaintext = new byte[uPlaintextSize];

		AutoSeededRandomPool rng;
		DecodingResult Result = Decryptor->Decrypt(rng, pCiphertext, uCiphertextSize, pPlaintext);
		uPlaintextSize = Result.messageLength;

		delete Decryptor;
		return Result.isValidCoding;
	}
	catch(...)
	{
		ASSERT(0);
		delete Decryptor;
		return false;
	}
}

bool CPrivateKey::Decrypt(CBuffer* pCiphertext, CBuffer* pPlaintext, UINT eAlgorithm)
{
	byte* pPlaintextBuff = NULL;
	size_t uPlaintextSize = 0;
	bool bRet = Decrypt(pCiphertext->GetBuffer(), pCiphertext->GetSize(), pPlaintextBuff, uPlaintextSize, eAlgorithm);
	if(pPlaintext)
		pPlaintext->SetBuffer(pPlaintextBuff, uPlaintextSize);
	else
		pCiphertext->SetBuffer(pPlaintextBuff, uPlaintextSize);
	return bRet;
}

bool CPrivateKey::GenerateKey(size_t uSize)
{
	byte PrivKeyArr[10240];
	ArraySink PrivKeySink(PrivKeyArr, ARRSIZE(PrivKeyArr));
	AutoSeededRandomPool rng;
	try
	{
		switch(m_eAlgorithm & eAsymCipher)
		{
			case eRSA:
			{
				InvertibleRSAFunction PrivKey;
				PrivKey.Initialize(rng,(int)(uSize*8));
				PrivKey.DEREncode(PrivKeySink);
				break;
			}
			case eLUCX:
			{
				InvertibleLUCFunction PrivKey;
				PrivKey.Initialize(rng,(int)(uSize*8));
				PrivKey.DEREncode(PrivKeySink);
				break;
			}
			case eLUC:
			{
				DL_CryptoKeys_LUC::PrivateKey PrivKey;
				PrivKey.Initialize(rng, (int)(uSize*8));
				PrivKey.DEREncode(PrivKeySink);
				break;
			}
			case eRabin:
			{
				InvertibleRabinFunction PrivKey;
				PrivKey.Initialize(rng,(int)(uSize*8));
				PrivKey.DEREncode(PrivKeySink);
				break;
			}
			case eDL:
			{
				DL_CryptoKeys_GFP::PrivateKey PrivKey;
				PrivKey.Initialize(rng,(int)(uSize*8));
				PrivKey.DEREncode(PrivKeySink);
				break;
			}
			default: throw 0;
		}
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	SetKey(PrivKeyArr,PrivKeySink.TotalPutLength());
	return true;
}

bool CPrivateKey::GenerateKey(const string& Curve)
{
	pOID pCurve = GetECCurve(Curve);
	if(!pCurve)
	{
		ASSERT(0);
		return false;
	}

	byte PrivKeyArr[10240];
	ArraySink PrivKeySink(PrivKeyArr, ARRSIZE(PrivKeyArr));
	AutoSeededRandomPool rng;
	try
	{
		switch(m_eAlgorithm & eAsymCipher)
		{
			case eECP: // GF(p)
			{
				DL_PrivateKey_EC<ECP> PrivKey;
				PrivKey.Initialize(rng, pCurve());
				PrivKey.AccessGroupParameters().SetEncodeAsOID(true);
				PrivKey.DEREncode(PrivKeySink);
				break;
			}
			case eEC2N: // GF(2^n)
			{
				DL_PrivateKey_EC<EC2N> PrivKey;
				PrivKey.Initialize(rng, pCurve());
				PrivKey.AccessGroupParameters().SetEncodeAsOID(true);
				PrivKey.DEREncode(PrivKeySink);
				break;
			}
			default: throw 0;
		}
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	SetKey(PrivKeyArr,PrivKeySink.TotalPutLength());
	return true;
}

CPublicKey* CPrivateKey::PublicKey() const
{
	ArraySource PrivKeySource(GetKey(),GetSize(),true,0);

	byte PubKeyArr[10240];
	ArraySink PubKeySink(PubKeyArr, ARRSIZE(PubKeyArr));
	try
	{
		switch(m_eAlgorithm & eAsymCipher)
		{
			case eRSA:
			{
				InvertibleRSAFunction PrivKey;
				PrivKey.BERDecode(PrivKeySource);

				RSAFunction PubKey;
				PubKey.Initialize(PrivKey.GetModulus(), PrivKey.GetPublicExponent());
				PubKey.DEREncode(PubKeySink);
				break;
			}
			case eLUCX:
			{
				InvertibleLUCFunction PrivKey;
				PrivKey.BERDecode(PrivKeySource);

				LUCFunction PubKey;
				PubKey.Initialize(PrivKey.GetModulus(), PrivKey.GetPublicExponent());
				PubKey.DEREncode(PubKeySink);
				break;
			}
			case eLUC:
			{
				DL_CryptoKeys_LUC::PrivateKey PrivKey;
				PrivKey.BERDecode(PrivKeySource);

				DL_CryptoKeys_LUC::PublicKey PubKey;
				PrivKey.MakePublicKey(PubKey);
				PubKey.DEREncode(PubKeySink);
				break;
			}
			case eRabin:
			{
				InvertibleRabinFunction PrivKey;
				PrivKey.BERDecode(PrivKeySource);

				RabinFunction PubKey;
				PubKey.Initialize(PrivKey.GetModulus(), PrivKey.GetQuadraticResidueModPrime1(), PrivKey.GetQuadraticResidueModPrime2());
				PubKey.DEREncode(PubKeySink);
				break;
			}
			case eDL:
			{
				DL_CryptoKeys_GFP::PrivateKey PrivKey;
				PrivKey.BERDecode(PrivKeySource);

				DL_CryptoKeys_GFP::PublicKey PubKey;
				PrivKey.MakePublicKey(PubKey);
				PubKey.DEREncode(PubKeySink);
				break;
			}
			case eECP: // GF(p)
			{
				DL_PrivateKey_EC<ECP> PrivKey;
				PrivKey.BERDecode(PrivKeySource);

				DL_PublicKey_EC<ECP> PubKey;
				PrivKey.MakePublicKey(PubKey);
				PubKey.AccessGroupParameters().SetEncodeAsOID(true);
				PubKey.DEREncode(PubKeySink);
				break;
			}
			case eEC2N: // GF(2^n)
			{
				DL_PrivateKey_EC<EC2N> PrivKey;
				PrivKey.BERDecode(PrivKeySource);

				DL_PublicKey_EC<EC2N> PubKey;
				PrivKey.MakePublicKey(PubKey);
				PubKey.AccessGroupParameters().SetEncodeAsOID(true);
				PubKey.DEREncode(PubKeySink);
				break;
			}
			default: throw 0;
		}
	}
	catch(...)
	{
		ASSERT(0);
		return NULL;
	}

	CPublicKey* pKey = new CPublicKey(m_eAlgorithm);
	pKey->SetKey(PubKeyArr, PubKeySink.TotalPutLength());
	return pKey;
}
