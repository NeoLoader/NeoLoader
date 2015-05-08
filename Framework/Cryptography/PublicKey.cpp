#include "GlobalHeader.h"
#include "AsymmetricKey.h"
#include "HashFunction.h"

using namespace CryptoPP;

CPublicKey::CPublicKey(UINT eAlgorithm)
{
	m_eAlgorithm = eAlgorithm;
}

bool CPublicKey::SetupKey(byte* pKey, size_t uSize)
{
	if(m_eAlgorithm == eUndefined)
	{
		try
		{
			ArraySource bt(pKey,uSize,true,0);

			BERSequenceDecoder subjectPublicKeyInfo(bt);

				BERSequenceDecoder algorithm(subjectPublicKeyInfo);
				OID oid(algorithm);
				algorithm.SkipAll();
				algorithm.MessageEnd();

			subjectPublicKeyInfo.SkipAll();
			subjectPublicKeyInfo.MessageEnd();

			if(RSAFunction().GetAlgorithmID() == oid)
				m_eAlgorithm  = eRSA;
			else if(DL_CryptoKeys_LUC::PublicKey().GetAlgorithmID() == oid)
				m_eAlgorithm  = eLUC;
			else if(DL_CryptoKeys_GFP::PublicKey().GetAlgorithmID() == oid)
				m_eAlgorithm  = eDL;
			else if(DL_PublicKey_EC<ECP>().GetAlgorithmID() == oid)
				m_eAlgorithm  = eECP;
			else if(DL_PublicKey_EC<EC2N>().GetAlgorithmID() == oid)
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

GET_H(CPublicKey, Verifier)
GET_SS(CPublicKey, Verifier)
GET_HP(CPublicKey, Verifier)
GET_SSP(CPublicKey, Verifier)

PK_Verifier* CPublicKey::GetVerifier(UINT eAlgorithm)
{
	if((eAlgorithm & eAsymMode) == ePKCS || ((eAlgorithm & eAsymMode) == 0  && ((eAlgorithm & eAsymCipher) == eRSA || (eAlgorithm & eAsymCipher) == eLUCX)))
		return GetVerifierPL1(eAlgorithm);
	return GetVerifierL1(eAlgorithm);
}

bool CPublicKey::Verify(byte* pPayload, size_t uPayloadSize, byte* pSignature, size_t uSignatureSize, UINT eAlgorithm)
{
	if((eAlgorithm & eAsymCipher) == 0)
		eAlgorithm |= (m_eAlgorithm & eAsymCipher);
	else if((eAlgorithm & eAsymCipher) != m_eAlgorithm)
	{
		ASSERT(0); // key incompatible!
		return false;
	}

	PK_Verifier* Verifier = NULL;
	try
	{
		Verifier = GetVerifier(eAlgorithm);
		ArraySource KeySource(GetKey(),GetSize(),true,0);
		Verifier->BERDecode(KeySource);
		
		bool Result = Verifier->VerifyMessage(pPayload, uPayloadSize, pSignature, uSignatureSize);
		delete Verifier;
		return Result;
	}
	catch(...)
	{
		ASSERT(0);
		delete Verifier;
		return false;
	}
}

bool CPublicKey::Verify(CBuffer* pPayload, CBuffer* pSignature, UINT eAlgorithm)
{
	return Verify(pPayload->GetBuffer(), pPayload->GetSize(), pSignature->GetBuffer(), pSignature->GetSize(), eAlgorithm);
}

GET_H(CPublicKey, Encryptor)
GET_ES(CPublicKey, Encryptor)

PK_Encryptor* CPublicKey::GetEncryptor(UINT eAlgorithm)
{
	return GetEncryptorL1(eAlgorithm);
}

bool CPublicKey::Encrypt(byte* pPlaintext, size_t uPlaintextSize, byte*& pCiphertext, size_t& uCiphertextSize, UINT eAlgorithm)
{
	if((eAlgorithm & eAsymCipher) == 0)
		eAlgorithm |= (m_eAlgorithm & eAsymCipher);
	else if((eAlgorithm & eAsymCipher) != m_eAlgorithm)
	{
		ASSERT(0); // key incompatible!
		return false;
	}

	PK_Encryptor* Encryptor = NULL;
	try
	{
		Encryptor = GetEncryptor(eAlgorithm);
		ArraySource KeySource(GetKey(),GetSize(),true,0);
		Encryptor->BERDecode(KeySource);
		
		uCiphertextSize = Encryptor->CiphertextLength(uPlaintextSize);
		ASSERT(uCiphertextSize > 0); // plaintext to long!
		if(uCiphertextSize == 0) 
			return false;
		ASSERT(pCiphertext == NULL);
		pCiphertext = new byte[uCiphertextSize];
	
		AutoSeededRandomPool rng;
		Encryptor->Encrypt(rng, pPlaintext, uPlaintextSize, pCiphertext);

		delete Encryptor;
		return true;
	}
	catch(...)
	{
		ASSERT(0);
		delete Encryptor;
		return false;
	}
}

bool CPublicKey::Encrypt(CBuffer* pPlaintext, CBuffer* pCiphertext, UINT eAlgorithm)
{
	byte* pCiphertextBuff = NULL;
	size_t uCiphertextSize = 0;
	if(!Encrypt(pPlaintext->GetBuffer(), pPlaintext->GetSize(), pCiphertextBuff, uCiphertextSize, eAlgorithm))
		return false;
	if(pCiphertext)
		pCiphertext->SetBuffer(pCiphertextBuff, uCiphertextSize);
	else
		pPlaintext->SetBuffer(pCiphertextBuff, uCiphertextSize);
	return true;
}

UINT CPublicKey::GetDefaultSS(UINT eAlgorithm)
{
	switch(eAlgorithm & eAsymCipher)
	{
		case eRSA:		return ePKCS; //ePSSR, ePSS
		case eLUCX:		return ePKCS; //ePSSR, ePSS
		case eLUC:		return eDSA;
		case eRabin:	return ePSSR; //ePKCS, ePSS
		case eDL:		return eDSA; //eNR
		case eECP:		return eDSA; //eNR
		case eEC2N:		return eDSA; //eNR
		default:ASSERT(0); return 0;
	}
}

UINT CPublicKey::GetDefaultES(UINT eAlgorithm)
{
	switch(eAlgorithm & eAsymCipher)
	{
		case eRSA:		return eOAEP; //ePKCS
		case eLUCX:		return eOAEP; //ePKCS
		case eLUC:		return eIES;
		case eRabin:	return eOAEP; //ePKCS
		case eDL:		return eIES;
		case eECP:		return eIES;
		case eEC2N:		return eIES;
		default:ASSERT(0); return 0;
	}
}
