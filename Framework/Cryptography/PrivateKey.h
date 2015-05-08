#pragma once

#include "AbstractKey.h"
#include "../Buffer.h"

class CPublicKey;

class NEOHELPER_EXPORT CPrivateKey: public CAbstractKey
{
public:
	CPrivateKey(UINT eAlgorithm = eUndefined);

	virtual bool			SetupKey(byte* pKey, size_t uSize);

	virtual	bool			Sign(byte* pPayload, size_t uPayloadSize, byte*& pSignature, size_t& uSignatureSize, UINT eAlgorithm = 0);
	virtual bool			Sign(CBuffer* pPayload, CBuffer* pSignature, UINT eAlgorithm = 0);
	virtual bool			Decrypt(byte* pCiphertext, size_t uCiphertextSize, byte*& pPlaintext, size_t& uPlaintextSize, UINT eAlgorithm = 0);
	virtual bool			Decrypt(CBuffer* pCiphertext, CBuffer* pPlaintext = NULL, UINT eAlgorithm = 0);

	virtual bool			GenerateKey(size_t uSize);
	virtual bool			GenerateKey(const string& Curve);

	virtual CPublicKey*		PublicKey() const;

protected:

private:
	CryptoPP::PK_Signer*	GetSignerL1(UINT eAlgorithm);
	CryptoPP::PK_Signer*	GetSignerPL1(UINT eAlgorithm);
	template<class H>
	CryptoPP::PK_Signer*	GetSignerL2(UINT eAlgorithm);
	template<class H>
	CryptoPP::PK_Signer*	GetSignerPL2(UINT eAlgorithm);
	CryptoPP::PK_Signer*	GetSigner(UINT eAlgorithm);

	CryptoPP::PK_Decryptor*	GetDecryptorL1(UINT eAlgorithm);
	template<class H>
	CryptoPP::PK_Decryptor*	GetDecryptorL2(UINT eAlgorithm);
	CryptoPP::PK_Decryptor*	GetDecryptor(UINT eAlgorithm);
};
