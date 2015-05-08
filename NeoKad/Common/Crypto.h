#pragma once

class CKeyExchange;
class CSymmetricKey;
class CAbstractKey;

UINT PrepSC(UINT eAlgorithm);
UINT ParseSC(const string& sSC);

CSymmetricKey* NewSymmetricKey(UINT eAlgorithm, CAbstractKey* pKey, CAbstractKey* pInIV, CAbstractKey* pOutIV);

UINT PrepKA(UINT eAlgorithm, string& Param);
UINT ParseKA(const string& sKA, string& Param);

CKeyExchange* NewKeyExchange(UINT eExAlgorithm, const string& Param);
