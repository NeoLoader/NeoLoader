#pragma once

#include "AbstractKey.h"
#include "../Buffer.h"

class NEOHELPER_EXPORT CPublicKey: public CAbstractKey
{
public:
	CPublicKey(UINT eAlgorithm = eUndefined);

	virtual bool			SetupKey(byte* pKey, size_t uSize);

	virtual	bool			Verify(byte* pPayload, size_t uPayloadSize, byte* pSignature, size_t uSignatureSize, UINT eAlgorithm = 0);
	virtual bool			Verify(CBuffer* pPayload, CBuffer* pSignature, UINT eAlgorithm = 0);
	virtual bool			Encrypt(byte* pPlaintext, size_t uPlaintextSize, byte*& pCiphertext, size_t& uCiphertextSize, UINT eAlgorithm = 0);
	virtual bool			Encrypt(CBuffer* pPlaintext, CBuffer* pCiphertext = NULL, UINT eAlgorithm = 0);

	static UINT				GetDefaultSS(UINT eAlgorithm);
	static UINT				GetDefaultES(UINT eAlgorithm);

protected:

private:
	CryptoPP::PK_Verifier*	GetVerifierL1(UINT eAlgorithm);
	CryptoPP::PK_Verifier*	GetVerifierPL1(UINT eAlgorithm);
	template<class H>
	CryptoPP::PK_Verifier*	GetVerifierL2(UINT eAlgorithm);
	template<class H>
	CryptoPP::PK_Verifier*	GetVerifierPL2(UINT eAlgorithm);
	CryptoPP::PK_Verifier*	GetVerifier(UINT eAlgorithm);

	CryptoPP::PK_Encryptor*	GetEncryptorL1(UINT eAlgorithm);
	template<class H>
	CryptoPP::PK_Encryptor*	GetEncryptorL2(UINT eAlgorithm);
	CryptoPP::PK_Encryptor*	GetEncryptor(UINT eAlgorithm);
};
