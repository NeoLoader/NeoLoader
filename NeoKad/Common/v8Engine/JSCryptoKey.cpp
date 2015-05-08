#include "GlobalHeader.h"
#include "JSCryptoKey.h"
#include "JSBuffer.h"
#include "JSVariant.h"

#include "../../../Framework/Cryptography/SymmetricKey.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"
#include "../../../Framework/Cryptography/KeyExchange.h"
#include "../../../Framework/Cryptography/HashFunction.h"
#include "../../../Framework/Strings.h"
#include "../../../Framework/Scope.h"

IMPLEMENT_OBJECT(CCryptoKeyObj, CObject)

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSSymKey::m_Template;

v8::Local<v8::ObjectTemplate> CJSSymKey::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"make"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxMake),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isValid"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxIsValid),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"encrypt"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxEncrypt),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"decrypt"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxDecrypt),v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"value"), GetValue, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"algorithm"), GetAlgorithm, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSSymKey::JSCryptoKey);

	return HandleScope.Escape(ConstructorTemplate);
}

CCryptoKeyObj* CJSSymKey::NewObj(const v8::FunctionCallbackInfo<v8::Value> &args, bool bReadAlgo)
{
	CScoped<CCryptoKeyObj> pCryptoKey = new CCryptoKeyObj();
	if(args.Length() >= 1)
	{
		bool bOK = false;

		UINT eAlgorithm = CAbstractKey::eUndefined;
		int DataIndex = -1;
		if(args[0]->IsString())
		{
			pair<wstring,wstring> Str = Split2(CJSEngine::GetWStr(args[0]),L"\\");
			if(!Str.second.empty() || args.Length() >= 2) // if we have a combo string or a second parameter
			{
				eAlgorithm = CAbstractKey::Str2Algorithm(w2s(Str.first)); // we reas the algorythm
				Str.first = Str.second;
			}
			else if(bReadAlgo) // we dont have algorythm, but we need it
				return NULL;

			if(!Str.first.empty())
			{
				CBuffer Key = FromHex(Str.first);
				bOK = pCryptoKey->SetKey(Key.GetBuffer(), Key.GetSize());
			}
			else 
				DataIndex = 1;
		}
		else if(!bReadAlgo)
			DataIndex = 0;

		if(DataIndex != -1 && args.Length() >= DataIndex && args[DataIndex]->IsObject())
		{
			if(CVariantPrx* pVariant = GetCObject<CVariantPrx>(args[DataIndex]->ToObject()))
			{
				CVariant Variant = pVariant->GetCopy();
				bOK = pCryptoKey->SetKey(Variant.GetData(), Variant.GetSize());
			}
			else if(CBufferObj* pBuffer = GetCObject<CBufferObj>(args[DataIndex]->ToObject()))
				bOK = pCryptoKey->SetKey(pBuffer->GetBuffer(), pBuffer->GetSize());
		}

		pCryptoKey->SetAlgorithm(eAlgorithm);
		if(!bOK) // caller throws exception on NULL
			return NULL;
	}
	return pCryptoKey.Detache();
}

/** new SymKey:
* Creates a new SymKey object
* 
* @param value
*	optional key value (buffer, variant or hex string representation)
*
* @param value
*	optional Algorithm string for buffer or variant input
*
* @return object
*	New SymKey object
*/
void CJSSymKey::JSCryptoKey(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSScript* jScript = GetJSObject<CJSScript>(args.Holder());
	if(CCryptoKeyObj* pCryptoKey = NewObj(args))
	{
		CJSSymKey* jObject = new CJSSymKey(pCryptoKey, jScript);
		args.GetReturnValue().Set(jObject->GetInstance());
	}
	else
		args.GetReturnValue().SetNull();
}

/** make:
* Creates a new symetric key
* 
* @param string Algorithm
*	Algorithm, can be "" - undefined
*
* @param int length
*	key length in bytes
*/
void CJSSymKey::FxMake(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CCryptoKeyObj* pCryptoKey = GetCObject<CCryptoKeyObj>(args.Holder());

	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	UINT eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[0]));
	pCryptoKey->SetAlgorithm(eAlgorithm);
	
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Key Size Missing"));
		return;
	}

	CAbstractKey Key(args[1]->Uint32Value(), true);
	pCryptoKey->SetKey(&Key);

	args.GetReturnValue().SetUndefined();
}

/** isValid:
* encrypt a buffer
* 
* @return true if the key is valid
*	encrpyted buffer
*/
void CJSSymKey::FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSSymKey* jCryptoKey = GetJSObject<CJSSymKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	UINT eAlgorithm = pCryptoKey->GetAlgorithm();
	CScoped<CEncryptionKey> pKey = CEncryptionKey::Make(eAlgorithm);
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pKey && pKey->SetKey(pCryptoKey)));
}

/** encrypt:
* encrypt a buffer
* 
* @param buffer Object
*	buffer to be encrypted
*
* @param IV 
*	Initialisation vector
*
* @return buffer object
*	encrpyted buffer
*/
void CJSSymKey::FxEncrypt(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSSymKey* jCryptoKey = GetJSObject<CJSSymKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}

	CBuffer Data;
	if ((pCryptoKey->GetAlgorithm() & CAbstractKey::eSymCipher) == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Alghorytm"));
		return;
	}
	
	CBufferObj* pIV = NULL;
	if(args.Length() >= 2)
	{
		if (!args[1]->IsObject())
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
			return;
		}
		pIV = GetCObject<CBufferObj>(args[1]->ToObject());
	}

	UINT eAlgorithm = pCryptoKey->GetAlgorithm();
	CScoped<CEncryptionKey> pKey = CEncryptionKey::Make(eAlgorithm);
	if (!pKey || !pKey->SetKey(pCryptoKey))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Key"));
		return;
	}
	if (!pKey->Setup(pIV ? pIV->GetBuffer() : NULL, pIV ? pIV->GetSize() : 0))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Key setup failed (IV invalid?)"));
		return;
	}
	pKey->Process(pData,&Data);
	
	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Data),jCryptoKey->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

/** decrypt:
* decrypt a buffer
* 
* @param buffer Object
*	buffer to be decrypted
*
* @param IV 
*	Initialisation vector
*
* @return buffer object
*	decrypted buffer
*/
void CJSSymKey::FxDecrypt(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSSymKey* jCryptoKey = GetJSObject<CJSSymKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}

	CBuffer Data;
	if ((pCryptoKey->GetAlgorithm() & CAbstractKey::eSymCipher) == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Alghorytm"));
		return;
	}

	CBufferObj* pIV = NULL;
	if (args.Length() >= 2)
	{
		if (!args[1]->IsObject())
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
			return;
		}
		pIV = GetCObject<CBufferObj>(args[1]->ToObject());
	}

	UINT eAlgorithm = pCryptoKey->GetAlgorithm();
	CScoped<CDecryptionKey> pKey = CDecryptionKey::Make(eAlgorithm);
	pKey->SetKey(pCryptoKey);
	if (!pKey->Setup(pIV ? pIV->GetBuffer() : NULL, pIV ? pIV->GetSize() : 0))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Key setup failed (IV invalid?)"));
		return;
	}
	pKey->Process(pData,&Data);

	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Data),jCryptoKey->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

/** value:
* return key value as buffer
* 
* @return buffer object
*	key value
*/
void CJSSymKey::GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CCryptoKeyObj* pCryptoKey = GetCObject<CCryptoKeyObj>(info.Holder());
	CJSObject* jNewObject = CJSBuffer::New(new CBufferObj(CBuffer(pCryptoKey->GetKey(), pCryptoKey->GetSize(), true)), GetScript(info.Holder()));
	info.GetReturnValue().Set(jNewObject->GetInstance());
}

/** algorithm:
* return key algorithm as string
* 
* @return string
*	key algorithm
*/
void CJSSymKey::GetAlgorithm(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CCryptoKeyObj* pCryptoKey = GetCObject<CCryptoKeyObj>(info.Holder());
	info.GetReturnValue().Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)CAbstractKey::Algorithm2Str(pCryptoKey->GetAlgorithm()).c_str()));
}

/** toString:
* return key value as hex string
* 
* @return string
*	key value
*/
void CJSSymKey::FxToString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CCryptoKeyObj* pCryptoKey = GetCObject<CCryptoKeyObj>(args.Holder());
	wstring Str = s2w(CAbstractKey::Algorithm2Str(pCryptoKey->GetAlgorithm())) + L"\\" + ToHex(pCryptoKey->GetKey(), pCryptoKey->GetSize());
	args.GetReturnValue().Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Str.c_str())));
}


///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSPrivKey::m_Template;

v8::Local<v8::ObjectTemplate> CJSPrivKey::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"make"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxMake),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isValid"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxIsValid),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"decrypt"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxDecrypt),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"sign"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSign),v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"pubKey"), GetPubKey, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"value"), CJSSymKey::GetValue, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"algorithm"), CJSSymKey::GetAlgorithm, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSSymKey::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSPrivKey::JSCryptoKey);

	return HandleScope.Escape(ConstructorTemplate);
}

/** new PrivKey:
* Creates a new PrivKey object
* 
* @param value
*	optional key value (buffer, variant or hex string representation)
*
* @return object
*	New PrivKey object
*/
void CJSPrivKey::JSCryptoKey(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSScript* jScript = GetJSObject<CJSScript>(args.Holder());
	if (CCryptoKeyObj* pCryptoKey = CJSSymKey::NewObj(args, false))
	{
		CJSPrivKey* jObject = new CJSPrivKey(pCryptoKey, jScript);
		args.GetReturnValue().Set(jObject->GetInstance());
	}
	else
		args.GetReturnValue().SetNull();
}

/** make:
* Creates a new private key
* 
* @param string Algorithm
*
* @param int length / string curv name
*	key length in bytes, or OID curve name for EC crypto
*/
void CJSPrivKey::FxMake(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CCryptoKeyObj* pCryptoKey = GetCObject<CCryptoKeyObj>(args.Holder());

	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	
	UINT eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[0]));
	pCryptoKey->SetAlgorithm(eAlgorithm);

	UINT eAsymCipher = eAlgorithm & CAbstractKey::eAsymCipher;
	UINT eAsymMode = eAlgorithm & CAbstractKey::eAsymMode;

	if (eAsymCipher == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CScoped<CPrivateKey> pPrivKey = new CPrivateKey(eAsymCipher);
	switch(eAsymMode)
	{
		case 0:
		case CAbstractKey::eECP:
		case CAbstractKey::eEC2N:
		{
			string Curve;
			if(args.Length() < 2)
			{
				switch(eAsymCipher)
				{
					case CAbstractKey::eECP:	Curve = "secp256r1"; break;
					case CAbstractKey::eEC2N:	Curve = "sect283k1"; break;
				}
			}
			else
				Curve = CJSEngine::GetStr(args[1]);

			if (!pPrivKey->GenerateKey(Curve))
			{
				args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Key Generation Failed (invalid curve?)"));
				return;
			}
			break;
		}
		default:
		{
			if (args.Length() < 2)
			{
				args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Key Size Missing"));
				return;
			}

			if (!pPrivKey->GenerateKey(args[1]->Uint32Value()))
			{
				args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Key Generation Failed"));
				return;
			}
		}
	}
	pCryptoKey->SetKey(pPrivKey);

	args.GetReturnValue().SetUndefined();
}

/** pubKey:
* return public key for this private key
* 
* @return key object
*	public key object
*/
void CJSPrivKey::GetPubKey(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPrivKey* jCryptoKey = GetJSObject<CJSPrivKey>(info.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	CScoped<CPrivateKey> pPrivKey = new CPrivateKey(pCryptoKey->GetAlgorithm());
	if (!pPrivKey->SetKey(pCryptoKey))
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Key"));
		return;
	}
	
	CScoped<CPublicKey> pPubKey = pPrivKey->PublicKey();
	CCryptoKeyObj* pNewKey = new CCryptoKeyObj();
	pNewKey->SetAlgorithm(pPubKey->GetAlgorithm());
	pNewKey->SetKey(pPubKey);

	CJSPubKey* jObject = new CJSPubKey(pNewKey, jCryptoKey->m_pScript);
	info.GetReturnValue().Set(jObject->GetInstance());
}

/** isValid:
* encrypt a buffer
* 
* @return true if the key is valid
*	encrpyted buffer
*/
void CJSPrivKey::FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPrivKey* jCryptoKey = GetJSObject<CJSPrivKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	CPrivateKey Key(pCryptoKey->GetAlgorithm());
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Key.SetKey(pCryptoKey)));
}

/** decrypt:
* decrypt a buffer
* 
* @param buffer Object
*	buffer to be decrypted
*
* @param string Algorithm 
*	optional alternative Algorithm to be used
*
* @return buffer object
*	decrypted buffer
*/
void CJSPrivKey::FxDecrypt(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPrivKey* jCryptoKey = GetJSObject<CJSPrivKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}

	CBuffer Data;
	if ((pCryptoKey->GetAlgorithm() & CAbstractKey::eAsymCipher) == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Incompatible Alghorytm"));
		return;
	}

	UINT eAlgorithm = 0;
	if(args.Length() >= 2)
		eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[1]));

	CPrivateKey Key(pCryptoKey->GetAlgorithm());
	if (!Key.SetKey(pCryptoKey))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Key"));
		return;
	}
	if (!Key.Decrypt(pData, &Data, eAlgorithm))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Alghorytm"));
		return;
	}

	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Data),jCryptoKey->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

/** sign:
* sign a buffer
* 
* @param buffer Object
*	buffer to be signed
*
* @param string Algorithm 
*	optional alternative Algorithm to be used
*
* @return buffer object
*	signature buffer
*/
void CJSPrivKey::FxSign(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPrivKey* jCryptoKey = GetJSObject<CJSPrivKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}

	UINT eAlgorithm = 0;
	if(args.Length() >= 2)
		eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[1]));

	CBuffer Sign;
	if ((pCryptoKey->GetAlgorithm() & CAbstractKey::eAsymCipher) == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Incompatible Alghorytm"));
		return;
	}
	
	CPrivateKey Key(pCryptoKey->GetAlgorithm());
	if (!Key.SetKey(pCryptoKey))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Key"));
		return;
	}
	if (!Key.Sign(pData, &Sign, eAlgorithm))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Alghorytm"));
		return;
	}

	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Sign),jCryptoKey->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSPubKey::m_Template;

v8::Local<v8::ObjectTemplate> CJSPubKey::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isValid"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxIsValid),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"encrypt"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxEncrypt),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"verify"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxVerify),v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"value"), CJSSymKey::GetValue, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"algorithm"), CJSSymKey::GetAlgorithm, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSSymKey::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSPubKey::JSCryptoKey);

	return HandleScope.Escape(ConstructorTemplate);
}

/** new PubKey:
* Creates a new PubKey object
* 
* @param value
*	optional key value (buffer, variant or hex string representation)
*
* @return object
*	New PubKey object
*/
void CJSPubKey::JSCryptoKey(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSScript* jScript = GetJSObject<CJSScript>(args.Holder());
	if (CCryptoKeyObj* pCryptoKey = CJSSymKey::NewObj(args, false))
	{
		CJSPubKey* jObject = new CJSPubKey(pCryptoKey, jScript);
		args.GetReturnValue().Set(jObject->GetInstance());
	}
	else
		args.GetReturnValue().SetNull();
}

/** isValid:
* encrypt a buffer
* 
* @return true if the key is valid
*	encrpyted buffer
*/
void CJSPubKey::FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPubKey* jCryptoKey = GetJSObject<CJSPubKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	CPublicKey Key(pCryptoKey->GetAlgorithm());
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Key.SetKey(pCryptoKey)));
}


/** encrypt:
* encrypt a buffer
* 
* @param buffer Object
*	buffer to be encrypted
*
* @param string Algorithm 
*	optional alternative Algorithm to be used
*
* @return buffer object
*	encrpyted buffer
*/
void CJSPubKey::FxEncrypt(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPubKey* jCryptoKey = GetJSObject<CJSPubKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}

	CBuffer Data;
	if ((pCryptoKey->GetAlgorithm() & CAbstractKey::eAsymCipher) == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Alghorytm"));
		return;
	}
	
	UINT eAlgorithm = 0;
	if(args.Length() >= 2)
		eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[1]));

	CPublicKey Key(pCryptoKey->GetAlgorithm());
	if (!Key.SetKey(pCryptoKey))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Key"));
		return;
	}
	if (!Key.Encrypt(pData, &Data, eAlgorithm))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Alghorytm"));
		return;
	}

	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Data),jCryptoKey->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

/** verify:
* verify a buffer
* 
* @param buffer Object
*	buffer to be encrypted
*
* @param buffer Object
*	buffer to be encrypted
*
* @param string Algorithm 
*	optional alternative Algorithm to be used
*
* @return bool
*	verification result
*/
void CJSPubKey::FxVerify(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPubKey* jCryptoKey = GetJSObject<CJSPubKey>(args.Holder());
	CCryptoKeyObj* pCryptoKey = jCryptoKey->m_pCryptoKey;

	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}

	UINT eAlgorithm = 0;
	if(args.Length() >= 3)
		eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[1]));

	if ((pCryptoKey->GetAlgorithm() & CAbstractKey::eAsymCipher) == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Alghorytm"));
		return;
	}
	
	if (args.Length() < 2 || !args[1]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Signature Missing"));
		return;
	}

	CBufferObj* pSign = GetCObject<CBufferObj>(args[1]->ToObject());
	if (!pSign)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Sgnature Incompatible"));
		return;
	}

	CPublicKey Key(pCryptoKey->GetAlgorithm());
	if (!Key.SetKey(pCryptoKey))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Key"));
		return;
	}

	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Key.Verify(pData, pSign, eAlgorithm)));
}

