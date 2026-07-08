#pragma once

class ShaderValue {
public:
	ShaderValue() {};
	virtual ~ShaderValue() {};

	const char*	Name;
	UInt32		RegisterIndex;
	UInt32		RegisterCount;
};


class ShaderFloatValue : public ShaderValue {
public:
	ShaderFloatValue() { Value = nullptr; };
	virtual ~ShaderFloatValue() {};

	void GetValueFromConstantTable();

	D3DXVECTOR4*		Value;
	D3DXPARAMETER_TYPE	Type;
};


class ShaderTextureValue : public ShaderValue {
public:
	ShaderTextureValue() {
		Texture = nullptr;
		TexturePath = "";
	};
	virtual ~ShaderTextureValue() {};

	void GetSamplerStateString(ID3DXBuffer* ShaderSource, UINT32 Index);
	void GetTextureRecord();

	std::string SamplerString;
	std::string TexturePath;
	TextureRecord* Texture;
	TextureRecord::TextureRecordType Type;
};


class ShaderProgram {
public:
	ShaderProgram();
	virtual ~ShaderProgram();

	virtual void SetCT() = 0;
	virtual void CreateCT(ID3DXBuffer* ShaderSource, ID3DXConstantTable* ConstantTable) = 0;

	static bool FileExists(const char* path);
	static bool CheckPreprocessResult(const char* CachedPreprocessPath, ID3DXBuffer* ShaderSource);


	ShaderFloatValue*	FloatShaderValues;
	UInt32				FloatShaderValuesCount;
	ShaderTextureValue*	TextureShaderValues;
	UInt32				TextureShaderValuesCount;
};
