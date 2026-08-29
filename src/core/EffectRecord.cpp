/*
* Class that wraps an effect shader, in order to load it/render it/set constants.
*/
EffectRecord::EffectRecord(const char* effectName) {

	Name = effectName;

	Effect = NULL;
	Enabled = false;
}

/*Shader Values arrays are freed in the superclass Destructor*/
EffectRecord::~EffectRecord() {
	if (Effect) Effect->Release();
}

/*
 * Unbinds all textures for samplers.
 * Useful when recreating textures on the fly.
 */
void EffectRecord::ClearSampler(const char* TextureName, size_t Length) {
	ShaderTextureValue* Sampler;
	for (UInt32 c = 0; c < TextureShaderValuesCount; c++) {
		Sampler = &TextureShaderValues[c];
		if (!memcmp(Sampler->Name, TextureName, Length) && Sampler->Texture->Texture) {
			Sampler->Texture->Texture = nullptr;
		}
	}
}

void EffectRecord::ClearSamplerBindings() {
	for (UInt32 c = 0; c < TextureShaderValuesCount; c++) {
		if (TextureShaderValues[c].Texture)
			TextureShaderValues[c].Texture->Texture = nullptr;
	}
}

bool EffectRecord::OnLostDevice() {
	if (!Effect)
		return true;
	HRESULT result = Effect->OnLostDevice();
	return SUCCEEDED(result);
}

bool EffectRecord::OnResetDevice() {
	if (!Effect)
		return true;
	HRESULT result = Effect->OnResetDevice();
	return SUCCEEDED(result);
}

/*
 * Unload effects, allowing it to be reloaded from  a blank state.
 */
void EffectRecord::DisposeEffect() {
	if (Effect) Effect->Release();
	Effect = nullptr;

	delete[] FloatShaderValues;
	FloatShaderValues = nullptr;
	FloatShaderValuesCount = 0;

	delete[] TextureShaderValues;
	TextureShaderValues = nullptr;
	TextureShaderValuesCount = 0;

	Enabled = false;
}

//D3DX error buffers are not reliably NUL terminated and usually carry trailing padding
static void LogShaderErrors(const char* apStage, const char* apName, ID3DXBuffer* apErrors) {
	if (!apErrors || !apErrors->GetBufferPointer()) {
		Log("[shader] %s failed for %s (no compiler output)", apStage, apName);
		return;
	}
	const char* text = (const char*)apErrors->GetBufferPointer();
	int length = (int)apErrors->GetBufferSize();
	while (length > 0 && (text[length - 1] == '\0' || text[length - 1] == '\n' || text[length - 1] == '\r'))
		--length;
	Log("[shader] %s failed for %s: %.*s", apStage, apName, length, text);
}

/*
 * Compile and Load the Effect shader
 */
bool EffectRecord::LoadEffect() {
	bool success = false;

	ID3DXEffect* Effect = NULL;
	ID3DXEffectCompiler* Compiler = NULL;
	ID3DXBuffer* EffectSource = NULL;
	ID3DXBuffer* Errors = NULL;
	ID3DXBuffer* EffectBuffer = NULL;

	char BaseDirectory[MAX_PATH];
	char CacheDirectory[MAX_PATH];
	char EffectSourcePath[MAX_PATH];
	char EffectPreprocessedPath[MAX_PATH];
	char EffectCompiledPath[MAX_PATH];

	// Build filenames.
	strcpy(BaseDirectory, EffectsPath);
	strcpy(CacheDirectory, BaseDirectory);
	strcat(CacheDirectory, "Cache\\");

	strcpy(EffectSourcePath, BaseDirectory);
	strcat(EffectSourcePath, Name);
	strcat(EffectSourcePath, ".fx.hlsl");

	if (!FileExists(EffectSourcePath)) {
		Log("[shader] source missing for %s at %s", Name, EffectSourcePath);
		return success;
	}

	strcpy(EffectPreprocessedPath, CacheDirectory);
	strcat(EffectPreprocessedPath, Name);
	strcat(EffectPreprocessedPath, ".fx.hlsl");

	strcpy(EffectCompiledPath, CacheDirectory);
	strcat(EffectCompiledPath, Name);
	strcat(EffectCompiledPath, ".fx");

	HRESULT prepass = D3DXPreprocessShaderFromFileA(EffectSourcePath, NULL, NULL, &EffectSource, &Errors);
	if (prepass != D3D_OK)
		LogShaderErrors("preprocess", Name, Errors);
	if (prepass == D3D_OK) {
		bool Compile = !CheckPreprocessResult(EffectPreprocessedPath, EffectSource);
		bool CompiledExists = FileExists(EffectCompiledPath);

		if (!Compile && CompiledExists) {
			HRESULT loaded = D3DXCreateEffectFromFileA(TheRenderManager->renderer->device, EffectCompiledPath, NULL, NULL, D3DXFX_LARGEADDRESSAWARE, NULL, &Effect, &Errors);
			if (FAILED(loaded)) {
				//stale or mismatched cache, the compile branch below rebuilds it
				LogShaderErrors("cached load", Name, Errors);
				if (Effect) { Effect->Release(); Effect = NULL; }
				if (Errors) { Errors->Release(); Errors = NULL; }
			}
			else {
				success = true;
			}
		}

		if (Compile || !CompiledExists || !Effect) {
			// compile if option was enabled or compiled version not found
			HRESULT compiled = D3DXCreateEffectCompiler(
				(const char*)EffectSource->GetBufferPointer(), 
				EffectSource->GetBufferSize(), 
				NULL, 
				NULL, 
				NULL, 
				&Compiler, 
				&Errors
			);
			if (FAILED(compiled)) {
				LogShaderErrors("compiler creation", Name, Errors);
				if (Compiler) Compiler->Release();
				if (EffectSource) EffectSource->Release();
				if (Effect) Effect->Release();
				if (Errors) Errors->Release();
				Enabled = false;
				return false;
			}

			compiled = Compiler->CompileEffect(NULL, &EffectBuffer, &Errors);
			if (FAILED(compiled)) {
				LogShaderErrors("compile", Name, Errors);
				if (Compiler) Compiler->Release();
				if (EffectSource) EffectSource->Release();
				if (Effect) Effect->Release();
				if (Errors) Errors->Release();
				Enabled = false;
				return false;
			}

			if (EffectBuffer) {
				std::ofstream FileBinary(EffectCompiledPath, std::ios::out | std::ios::binary);
				FileBinary.write((const char*)EffectBuffer->GetBufferPointer(), EffectBuffer->GetBufferSize());
				FileBinary.flush();
				FileBinary.close();

				std::ofstream FilePreprocess(EffectPreprocessedPath, std::ios::out | std::ios::binary);
				FilePreprocess.write((const char*)EffectSource->GetBufferPointer(), EffectSource->GetBufferSize());
				FilePreprocess.flush();
				FilePreprocess.close();
			}

			HRESULT loaded = D3DXCreateEffectFromFileA(TheRenderManager->renderer->device, EffectCompiledPath, NULL, NULL, D3DXFX_LARGEADDRESSAWARE, NULL, &Effect, &Errors);
			if (FAILED(loaded)) {
				LogShaderErrors("load", Name, Errors);
				if (Compiler) Compiler->Release();
				if (EffectSource) EffectSource->Release();
				if (Effect) Effect->Release();
				if (Errors) Errors->Release();
				if (EffectBuffer) EffectBuffer->Release();
				Enabled = false;
				return false;
			}

			success = true;
		}
	}

	if (Effect) {
		this->Effect = Effect;
		CreateCT(EffectSource, NULL); //Create the object which will associate a register index to a float pointer for constants updates;
	}

	//ShaderManager::RegisterEffect sets Enabled after this returns
	Enabled = Effect != nullptr;
	if (!Effect)
		Log("[shader] %s produced no effect object, it will not render", Name);

	if (Compiler) Compiler->Release();
	if (Errors) Errors->Release();
	if (EffectBuffer) EffectBuffer->Release();
	if (EffectSource) EffectSource->Release();

	return success;
}


/**
* Loads an effect shader by name (The post process effects stored in the Effects folder)
* @param Name the name for the effect
* @returns an EffectRecord describing the effect shader.
*/
bool EffectRecord::IsLoaded() {
	return Effect != nullptr;
}


/**
Creates the Constant Table for the Effect Record.
*/
void EffectRecord::CreateCT(ID3DXBuffer* ShaderSource, ID3DXConstantTable* ConstantTable) {
	D3DXEFFECT_DESC ConstantTableDesc;
	D3DXPARAMETER_DESC ConstantDesc;
	D3DXHANDLE Handle;
	UINT ConstantCount = 1;
	UInt32 FloatIndex = 0;
	UInt32 TextureIndex = 0;

	Effect->GetDesc(&ConstantTableDesc);
	for (UINT c = 0; c < ConstantTableDesc.Parameters; c++) {
		Handle = Effect->GetParameter(NULL, c);
		Effect->GetParameterDesc(Handle, &ConstantDesc);
		if (memcmp(ConstantDesc.Name, "TESR_", 5)) continue;
		if ((ConstantDesc.Class == D3DXPC_VECTOR || ConstantDesc.Class == D3DXPC_MATRIX_ROWS || ConstantDesc.Class == D3DXPC_MATRIX_COLUMNS)) FloatShaderValuesCount += 1;
		if (ConstantDesc.Class == D3DXPC_OBJECT && ConstantDesc.Type >= D3DXPT_SAMPLER && ConstantDesc.Type <= D3DXPT_SAMPLERCUBE) TextureShaderValuesCount += 1;
	}
	FloatShaderValues = new ShaderFloatValue[FloatShaderValuesCount];
	TextureShaderValues = new ShaderTextureValue[TextureShaderValuesCount];

	for (UINT c = 0; c < ConstantTableDesc.Parameters; c++) {
		Handle = Effect->GetParameter(NULL, c);
		Effect->GetParameterDesc(Handle, &ConstantDesc);
		if (memcmp(ConstantDesc.Name, "TESR_", 5)) continue; // Only treat constants named with TESR prefix

		switch (ConstantDesc.Class) {
		case D3DXPC_VECTOR:
			FloatShaderValues[FloatIndex].Name = ConstantDesc.Name;
			FloatShaderValues[FloatIndex].Type = (D3DXPARAMETER_TYPE)D3DXPC_VECTOR;
			FloatShaderValues[FloatIndex].RegisterIndex = (UInt32)Handle;
			FloatShaderValues[FloatIndex].RegisterCount = ConstantDesc.Elements;
			FloatIndex++;
			break;
		case D3DXPC_MATRIX_ROWS:
		case D3DXPC_MATRIX_COLUMNS:
			FloatShaderValues[FloatIndex].Name = ConstantDesc.Name;
			FloatShaderValues[FloatIndex].Type = (D3DXPARAMETER_TYPE)ConstantDesc.Class;
			FloatShaderValues[FloatIndex].RegisterIndex = (UInt32)Handle;
			FloatShaderValues[FloatIndex].RegisterCount = ConstantDesc.Elements;
			FloatIndex++;
			break;
		case D3DXPC_OBJECT:
			if (ConstantDesc.Class == D3DXPC_OBJECT && ConstantDesc.Type >= D3DXPT_SAMPLER && ConstantDesc.Type <= D3DXPT_SAMPLERCUBE) {
				TextureShaderValues[TextureIndex].Name = ConstantDesc.Name;
				TextureShaderValues[TextureIndex].Type = TextureRecord::GetTextureType(ConstantDesc.Type);
				TextureShaderValues[TextureIndex].RegisterIndex = TextureIndex;
				TextureShaderValues[TextureIndex].RegisterCount = 1;
				TextureShaderValues[TextureIndex].GetSamplerStateString(ShaderSource, TextureIndex);
				TextureShaderValues[TextureIndex].GetTextureRecord();
				TextureIndex++;
			}
			break;
		}
	}
}

/*
*Sets the Effect Shader constants table and texture registers.
*/
void EffectRecord::SetCT() {
	if (!Enabled || Effect == nullptr) return;

	ShaderTextureValue* Sampler;
	for (UInt32 c = 0; c < TextureShaderValuesCount; c++) {
		Sampler = &TextureShaderValues[c];
		if (!Sampler->Texture->Texture)
			Sampler->Texture->BindTexture(Sampler->Name);

		if (Sampler->Texture->Texture != nullptr) {
			TheRenderManager->renderer->device->SetTexture(Sampler->RegisterIndex, Sampler->Texture->Texture);
			for (int i = 1; i < SamplerStatesMax; i++) {
				TheRenderManager->renderer->SetSamplerState(Sampler->RegisterIndex, (D3DSAMPLERSTATETYPE)i, Sampler->Texture->SamplerStates[i]);
			}
		}
	}

	ShaderFloatValue* Constant;
	for (UInt32 c = 0; c < FloatShaderValuesCount; c++) {
		Constant = &FloatShaderValues[c];
		if (Constant->Value == nullptr)  Constant->GetValueFromConstantTable();
		if (Constant->Value == nullptr)
			continue;

		if (Constant->Type == (D3DXPARAMETER_TYPE)D3DXPC_VECTOR) {
			if (Constant->RegisterCount > 1)
				Effect->SetVectorArray((D3DXHANDLE)Constant->RegisterIndex, Constant->Value, Constant->RegisterCount);
			else
				Effect->SetVector((D3DXHANDLE)Constant->RegisterIndex, Constant->Value);
		}
		else
			if (Constant->RegisterCount > 1)
				Effect->SetMatrixArray((D3DXHANDLE)Constant->RegisterIndex, (D3DXMATRIX*)Constant->Value, Constant->RegisterCount);
			else
				Effect->SetMatrix((D3DXHANDLE)Constant->RegisterIndex, (D3DXMATRIX*)Constant->Value);
	}
}

/*
 * Enable or Disable Effect, with loading if not loaded
 */
bool EffectRecord::SwitchEffect() {
	bool change = true;
	if (!IsLoaded() && !Enabled) {
		DisposeEffect();
		change = LoadEffect();
	}
	if (change) Enabled = !Enabled;
	return Enabled;
}


/**
* Renders the given effect shader.
*/
void EffectRecord::Render(IDirect3DDevice9* Device, IDirect3DSurface9* RenderTarget, IDirect3DSurface9* RenderedSurface, UINT techniqueIndex, bool ClearRenderTarget, IDirect3DSurface9* SourceBuffer) {

	if (!Enabled || Effect == nullptr || !ShouldRender())
		return; // skip rendering of disabled effects

	if (SourceBuffer) Device->StretchRect(RenderTarget, NULL, SourceBuffer, NULL, D3DTEXF_LINEAR);

	D3DXHANDLE technique = Effect->GetTechnique(techniqueIndex);
	HRESULT hr = Effect->SetTechnique(technique);
	if (FAILED(hr)) return;

	SetCT(); // update the constant table
	UINT Passes = 0;
	hr = Effect->Begin(&Passes, NULL);
	if (FAILED(hr)) return;

	for (UINT p = 0; p < Passes; p++) {
		if (ClearRenderTarget) Device->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0L);
		hr = Effect->BeginPass(p);
		if (FAILED(hr)) continue;
		Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		Effect->EndPass();
		if (RenderedSurface) Device->StretchRect(RenderTarget, NULL, RenderedSurface, NULL, D3DTEXF_LINEAR); // copy the result from the pass into the texture
	}
	Effect->End();
}
