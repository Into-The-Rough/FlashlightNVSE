#include "ShaderRecord.h"

ShaderProgram::ShaderProgram() {
	FloatShaderValues = NULL;
	TextureShaderValues = NULL;
	FloatShaderValuesCount = 0;
	TextureShaderValuesCount = 0;
}

ShaderProgram::~ShaderProgram() {
	delete[] FloatShaderValues;
	delete[] TextureShaderValues;
}

bool ShaderProgram::FileExists(const char* path) {
	WIN32_FILE_ATTRIBUTE_DATA attributes = { 0 };
	return GetFileAttributesExA(path, GetFileExInfoStandard, &attributes);
}

bool ShaderProgram::CheckPreprocessResult(const char* CachedPreprocessPath, ID3DXBuffer* ShaderSource) {
	void* CurrentContent = ShaderSource->GetBufferPointer();

	ID3DXBuffer* CachedSource = nullptr;
	void* CachedContent = nullptr;

	std::ifstream File(CachedPreprocessPath, std::ios::in | std::ios::ate);
	if (File.is_open()) {
		std::streamoff Size = File.tellg();
		if (Size > 0 && SUCCEEDED(D3DXCreateBuffer(Size, &CachedSource))) {
			File.seekg(0, std::ios::beg);
			CachedContent = CachedSource->GetBufferPointer();
			File.read((char*)CachedContent, Size);
			File.close();
		}
		else {
			File.close();
			return false;
		}
	}
	else {
		return false;
	}

	bool match = false;
	if (CachedContent && CachedSource) {
		if (ShaderSource->GetBufferSize() == CachedSource->GetBufferSize()) {
			match = !memcmp(CurrentContent, CachedContent, ShaderSource->GetBufferSize());
		}
	}

	if (CachedSource) CachedSource->Release();
	return match;
}


void ShaderTextureValue::GetSamplerStateString(ID3DXBuffer* ShaderSource, UINT32 Index) {
	std::string Source(
		(const char*)ShaderSource->GetBufferPointer(),
		ShaderSource->GetBufferSize()
	);

	//find the sampler line by NAME, not loop counter. our shaders skip
	//register slots, so the sampler index in the shader may not match the
	//enumeration order — looking up by name lets explicit register clauses
	//resolve correctly even if it's the 4th sampler enumerated.
	size_t NamePos = Source.find(Name);
	if (NamePos == std::string::npos)
		return;
	size_t SamplerPos = Source.find("register", NamePos);
	if (SamplerPos == std::string::npos)
		return;

	size_t SamplerEnd = Source.find("\n", SamplerPos + 1);
	std::string SamplerLine = Source.substr(SamplerPos, SamplerEnd - SamplerPos);

	//parse the actual sampler stage number from `register ( sN )` so the
	//SetTexture / SetSamplerState calls hit the correct D3D stage.
	size_t OpenParen = SamplerLine.find("(");
	size_t StagePos = OpenParen != std::string::npos ? SamplerLine.find("s", OpenParen + 1) : std::string::npos;
	if (StagePos != std::string::npos)
		RegisterIndex = (UInt32)atoi(SamplerLine.c_str() + StagePos + 1);

	size_t StartTexture = SamplerLine.find("<");
	size_t EndTexture = SamplerLine.rfind(">");
	if (StartTexture != std::string::npos && EndTexture != std::string::npos) {
		std::string TextureString = SamplerLine.substr(StartTexture + 1, EndTexture - StartTexture - 1);
		if (TextureString.find("ResourceName") != std::string::npos) {
			size_t StartPath = TextureString.find("\"");
			size_t EndPath = TextureString.rfind("\"");
			std::string PathS = TextureString.substr(StartPath + 1, EndPath - 1 - StartPath);
			PathS.insert(0, "Data\\Textures\\");
			TexturePath = PathS;
		}
	}

	size_t StartStatePos = SamplerLine.find("{");
	size_t EndStatePos = SamplerLine.rfind("}");
	if (EndStatePos == std::string::npos || StartStatePos == std::string::npos)
		return;
	SamplerString = SamplerLine.substr(StartStatePos + 1, EndStatePos - StartStatePos - 1);
}


void ShaderTextureValue::GetTextureRecord() {
	if (!Type)
		return;

	Texture = new TextureRecord();

	if (TexturePath != "") Texture->Texture = TheTextureManager->GetFileTexture(TexturePath, Type);
	if (!SamplerString.empty()) Texture->GetSamplerStates(trim(SamplerString));
}


void ShaderFloatValue::GetValueFromConstantTable() {
	std::string constantName = Name;
	auto iter = TheShaderManager->ConstantsTable.find(constantName);

	if (iter != TheShaderManager->ConstantsTable.end()) {
		Value = TheShaderManager->ConstantsTable.at(Name);
	}
	else {
		D3DXVECTOR4 v; v.x = v.y = v.z = v.w = 0.0f;
		TheShaderManager->CustomConst[Name] = v;
		Value = &TheShaderManager->CustomConst[Name];
	}
}
