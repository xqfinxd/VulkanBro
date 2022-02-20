#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

#include "glslang/Public/ShaderLang.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "tinyxml2.h"

void InitResources(TBuiltInResource& res);
std::string ReadFile(const char* name);
std::vector<uint32_t> GenerateSpv(const glslang::TIntermediate& intermediate);
std::string GetVecString(std::vector<uint32_t>& code, char separator = ',');
std::string GetDimString(const glslang::TType& ttype, char separator = ',');
const char* GetDimString(glslang::TSamplerDim dim);
std::vector<uint32_t> GetVecFromString(const char* srcString, char separator = ',');
EShLanguage GetStageByName(const char* stageName);
const char* GetStageName(EShLanguage stage);
std::string GetStagesString(EShLanguageMask mask, char separator = ',');
glslang::EShTargetClientVersion GetClientVersion(const char* verString);
const char* GetClientVersionString(glslang::EShTargetClientVersion version);
glslang::EShTargetLanguageVersion GetLanguageVersion(const char* verString);
const char* GetLanguageVersionString(glslang::EShTargetLanguageVersion version);

const int kGlslVersion = 400;
const glslang::EShSource kSourceLanguage = glslang::EShSourceGlsl;
const glslang::EShClient kClient = glslang::EShClientVulkan;
const glslang::EshTargetClientVersion kTargetClientVersion = glslang::EShTargetVulkan_1_0;
const glslang::EShTargetLanguage kTargetLanguage = glslang::EShTargetSpv;
const glslang::EShTargetLanguageVersion kTargetLanguageVersion = glslang::EShTargetSpv_1_0;
const EShMessages kMessages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDebugInfo );
const EShLanguage kInvalidStage = EShLangCount;

struct ProcessCheck {
	bool checked = false;
	ProcessCheck() {
		checked = glslang::InitializeProcess();
	}
	~ProcessCheck() {
		glslang::FinalizeProcess();
	}
	bool operator()() {
		if (!checked) {
			checked = glslang::InitializeProcess();
		}
		return checked;
	}
};

void CheckProcess() {
	static ProcessCheck processCheck{};
	if (!processCheck()) {
		throw "初始化进程失败！";
	}
}

class Program {
	friend class ShaderXmlWriter;
public:
	Program() {
		CheckProcess();
		program = new glslang::TProgram();
	}

	bool AddShader(const char* src, EShLanguage stage) {
		auto iter = shaders.find(stage);
		if (shaders.end() != iter) {
			std::cerr << "重复添加相同类型着色器！" << std::endl;
			return false;
		}
		glslang::TShader* shader = new glslang::TShader(stage);
		const char* vertStringList[] = { src };
		TBuiltInResource res;
		::InitResources(res);
		shader->setStrings(vertStringList, 1);
		shader->setEntryPoint("main");
		shader->setEnvInput(kSourceLanguage, stage, kClient, glslVersion);
		shader->setEnvClient(kClient, targetClientVersion);
		shader->setEnvTarget(kTargetLanguage, targetLanguageVersion);
		bool parseResult = shader->parse(&res, 1, true, kMessages);
		if (!parseResult) {
			std::cerr << shader->getInfoLog() << std::endl;
			return false;
		}
		shaders.insert(std::make_pair(stage, shader));
		return true;
	}

	bool AddShader(const char* file) {
		const char* dotPos = strrchr(file, '.');
		if (!dotPos || strlen(dotPos) <= 1) {
			std::cerr << file << "：错误的文件扩展名！" << std::endl;
			std::cerr << "\t期望的扩展名：*.vert, *.tesc, *.tese, *.geom, *.frag." << std::endl;
			return false;
		}

		const char* stageName = dotPos + 1;
		EShLanguage stage = ::GetStageByName(stageName);
		if (kInvalidStage == stage) {
			std::cerr << stageName << "：不支持的文件扩展名！" << std::endl;
			std::cerr << "\t期望的扩展名：*.vert, *.tesc, *.tese, *.geom, *.frag." << std::endl;
			return false;
		}

		std::string shaderString = ::ReadFile(file);
		if (shaderString.empty()) {
			std::cerr << file << "：文件读取失败或者文件为空！" << std::endl;
			return false;
		}
		return AddShader(shaderString.c_str(), stage);
	}

	bool Link() {
		for (const auto& shader : shaders) {
			program->addShader(shader.second);
		}
		bool linkResult = program->link(kMessages);
		if (!linkResult) {
			std::cerr << "着色器链接失败！" << std::endl;
			std::cerr << program->getInfoLog() << std::endl;
			return false;
		}
		bool buildResult = program->buildReflection();
		if (!buildResult) {
			std::cerr << "着色器分析失败！" << std::endl;
			std::cerr << program->getInfoLog() << std::endl;
			return false;
		}
		return true;
	}

	~Program() {
		for (auto& shader : shaders) {
			delete shader.second;
		}
		shaders.clear();
		delete program;
	}

#ifdef _DEBUG
	glslang::TIntermediate* GetShaderInfo(const char* stageName) const {
		auto stage = ::GetStageByName(stageName);
		if (kInvalidStage == stage) {
			return nullptr;
		}
		return program->getIntermediate(stage);
	}

	int GetBlockCount() const {
		return program->getNumUniformBlocks();
	}

	const glslang::TObjectReflection& GetBlock(int index) {
		return program->getUniformBlock(index);
	}

	int GetUniformCount() const {
		return program->getNumUniformVariables();
	}

	const glslang::TObjectReflection& GetUniform(int index) const {
		return program->getUniform(index);
	}

	int GetPipeInCount() const {
		return program->getNumPipeInputs();
	}

	const glslang::TObjectReflection& GetPipeIn(int index) const {
		return program->getPipeInput(index);
	}

	int GetPipeOutCount() const {
		return program->getNumPipeOutputs();
	}

	const glslang::TObjectReflection& GetPipeOut(int index) const {
		return program->getPipeOutput(index);
	}
#endif // DEBUG


private:
	glslang::TProgram* program = nullptr;
	std::map<EShLanguage, glslang::TShader*> shaders{};
	int glslVersion = 400;
	glslang::EShTargetClientVersion targetClientVersion = glslang::EShTargetVulkan_1_0;
	glslang::EShTargetLanguageVersion targetLanguageVersion = glslang::EShTargetSpv_1_0;
};

class ShaderXmlWriter {
public:
	ShaderXmlWriter(const char* fileName_) : fileName(fileName_) {
		document = new tinyxml2::XMLDocument();
	}
	~ShaderXmlWriter() {
		delete document;
	}

	bool LoadAsProgram(Program& program, const char* verName, const char* shaderName, bool afterClear) {
		auto res = document->LoadFile(fileName.c_str());
		if (tinyxml2::XML_SUCCESS != res) {
			return false;
		}
		auto rootNode = document->RootElement();
		// load version
		{
			auto verNode = rootNode->FirstChildElement(verName);
			if (!verNode) {
				return false;
			}
			program.glslVersion = kGlslVersion;
			const char* glslVer = verNode->Attribute("glsl");
			if (glslVer) {
				int newVer = atoi(glslVer);
				if (newVer) {
					program.glslVersion = newVer;
				}
			}

			const char* targetClientVer = verNode->Attribute("client");
			if (targetClientVer) {
				program.targetClientVersion = ::GetClientVersion(targetClientVer);
			}

			const char* targetLanguageVer = verNode->Attribute("language");
			if (targetLanguageVer) {
				program.targetLanguageVersion = ::GetLanguageVersion(targetLanguageVer);
			}
		}

		// load shader
		{
			bool shaderComplete = true;
			auto shaderNode = rootNode->FirstChildElement(shaderName);
			while (shaderNode) {
				EShLanguage stage = kInvalidStage;
				const char* stageName = shaderNode->Attribute("stage");
				if (stageName) {
					stage = ::GetStageByName(stageName);
				}
				if (kInvalidStage == stage) {
					shaderNode = shaderNode->NextSiblingElement(shaderName);
					continue;
				}
				auto srcNode = shaderNode->FirstChildElement("src");
				if (!srcNode) {
					shaderNode = shaderNode->NextSiblingElement(shaderName);
					continue;
				}
				const char* shaderSrc = srcNode->GetText();
				shaderComplete = shaderComplete && program.AddShader(shaderSrc, stage);
				shaderNode = shaderNode->NextSiblingElement(shaderName);
			}
			if (shaderComplete) {
				if (!program.Link()) {
					return false;
				}
			}
		}
		if (afterClear) {
			document->DeleteChildren();
		}
		return true;
	}

	void BeginWrite() {
		auto declration = document->NewDeclaration();
		document->LinkEndChild(declration);
		root = document->NewElement("shaderxml");
	}

	void EndWrite() {
		document->LinkEndChild(root);
		document->SaveFile(fileName.c_str());
	}

	void WriteVersion(const Program& program, const char* verName) {
		auto verNode = root->InsertNewChildElement(verName);
		verNode->SetAttribute("glsl", program.glslVersion);
		const char* clientVersionString = GetClientVersionString(program.targetClientVersion);
		if (!clientVersionString) {
			clientVersionString = ::GetClientVersionString(kTargetClientVersion);
		}
		verNode->SetAttribute("client", clientVersionString);
		const char* languageVersionString = GetLanguageVersionString(program.targetLanguageVersion);
		if (!languageVersionString) {
			languageVersionString = ::GetLanguageVersionString(kTargetLanguageVersion);
		}
		verNode->SetAttribute("language", languageVersionString);
	}
	
	void WriteShader(const char* nodeName, const glslang::TIntermediate& shaderInfo) {
		auto shaderNode = root->InsertNewChildElement(nodeName);
		const char* stageName = ::GetStageName(shaderInfo.getStage());
		shaderNode->SetAttribute("stage", stageName);
		shaderNode->SetAttribute("entry", shaderInfo.getEntryPointName().c_str());
		if (auto srcNode = shaderNode->InsertNewChildElement("src")) {
			srcNode->SetText(shaderInfo.getSourceText().c_str());
		} else {
			return;
		}
		if (auto spvNode = shaderNode->InsertNewChildElement("spv")) {
			spvNode->SetAttribute("separator", ",");
			auto spvCode = ::GenerateSpv(shaderInfo);
			auto spvString = ::GetVecString(spvCode);
			spvNode->SetText(spvString.c_str());
		}
	}

	void WriteChildren(tinyxml2::XMLElement* node, const char* memName, const char* subName, const glslang::TType& ttype) {
		tinyxml2::XMLElement* childNode = nullptr;
		if (ttype.isStruct()) {
			auto childNode = node->InsertNewChildElement(subName);
			childNode->SetAttribute("name", ttype.getTypeName().c_str());
			if (auto* structures = ttype.getStruct()) {
				for (const auto& st : *structures) {
					if (!st.type) {
						continue;
					}
					WriteChildren(childNode, memName, subName, *st.type);
				}
			}
		} else {
			auto childNode = node->InsertNewChildElement(memName);
			childNode->SetAttribute("name", ttype.getFieldName().c_str());
			childNode->SetAttribute("type", ttype.getBasicString());
			auto dimString = ::GetDimString(ttype);
			if (!dimString.empty()) {
				childNode->SetAttribute("dim", dimString.c_str());
			}
		}
	}

	void WriteUniform(const char* uniformName, const char* samplerName, const glslang::TObjectReflection& reflection) {
		auto* ttype = reflection.getType();
		if (!ttype || ttype->isStruct()) {
			return;
		}
		if (glslang::EbtSampler == ttype->getBasicType()) {
			auto uniformNode = root->InsertNewChildElement(samplerName);
			uniformNode->SetAttribute("name", reflection.name.c_str());
			uniformNode->SetAttribute("combine", ttype->getSampler().isCombined());
			uniformNode->SetAttribute("type", ttype->getBasicString(ttype->getSampler().getBasicType()));
			uniformNode->SetAttribute("dim", ::GetDimString(ttype->getSampler().dim));
		} else {
			auto uniformNode = root->InsertNewChildElement(uniformName);
			uniformNode->SetAttribute("name", reflection.name.c_str());
			if (reflection.offset >= 0) {
				uniformNode->SetAttribute("offset", reflection.offset);
			}
		}
		
	}

	void WriteBlock(const char* nodeName, const char* memName, const char* subName,const glslang::TObjectReflection& reflection) {
		auto* ttype = reflection.getType();
		if (!ttype || !ttype->isStruct()) {
			return;
		}
		const auto& qualifier = ttype->getQualifier();
		auto blockNode = root->InsertNewChildElement(nodeName);
		blockNode->SetAttribute("name", ttype->getTypeName().c_str());
		blockNode->SetAttribute("size", reflection.size);
		if (qualifier.hasSet()) {
			blockNode->SetAttribute("set", qualifier.layoutSet);
		} else {
			blockNode->SetAttribute("set", 0);
		}
		if (qualifier.hasBinding()) {
			blockNode->SetAttribute("binding", qualifier.layoutBinding);
		} else {
			blockNode->SetAttribute("binding", 0);
		}
		blockNode->SetAttribute("index", reflection.index);
		auto stagesString = ::GetStagesString(reflection.stages);
		blockNode->SetAttribute("stages", stagesString.c_str());
		if (auto childStructure = ttype->getStruct()) {
			for (const auto& structure : *childStructure) {
				if (!structure.type) {
					continue;
				}
				WriteChildren(blockNode, memName, subName, *structure.type);
			}
		}
	}

	void WritePipe(const char* nodeName, const glslang::TObjectReflection& reflection) {
		auto* ttype = reflection.getType();
		if (!ttype) {
			return;
		}
		const auto& qualifier = ttype->getQualifier();
		auto pipeNode = root->InsertNewChildElement(nodeName);
		pipeNode->SetAttribute("name", reflection.name.c_str());
		if (qualifier.hasLocation()) {
			pipeNode->SetAttribute("location", qualifier.layoutLocation);
		}
		pipeNode->SetAttribute("type", ttype->getBasicString());
		auto dimString = ::GetDimString(*ttype);
		if (!dimString.empty()) {
			pipeNode->SetAttribute("dim", dimString.c_str());
		}
		auto stagesString = ::GetStagesString(reflection.stages);
		if (!stagesString.empty()) {
			pipeNode->SetAttribute("stages", stagesString.c_str());
		}
	}

private:
	tinyxml2::XMLDocument* document = nullptr;
	tinyxml2::XMLElement* root = nullptr;
	std::string fileName{};
};

int main(int argc, char** argv) {
	Program program{};

	std::set<const char*> fileList{};
	int mode = 0;
	for (int i = 1; i < argc; ++i) {
		const char* argument = argv[i];
		if (strcmp(argument, "--genxml") == 0) {
			mode = 1;
		} else if(strcmp(argument, "--reload") == 0) {
			mode = 2;
		} else {
			fileList.insert(argument);
		}
	}
	if (mode == 1) {
		for (const auto* filePath : fileList) {
			bool shaderPass = program.AddShader(filePath);
			if (!shaderPass) {
				std::cerr << filePath << "：文件加载失败！" << std::endl;
				exit(1);
			}
		}
		bool linkPass = program.Link();
		if (!linkPass) {
			exit(2);
		}
		ShaderXmlWriter writer("shader.xml");
		writer.BeginWrite();
		writer.WriteVersion(program, "version");
		if (auto* shaderInfo = program.GetShaderInfo("vert")) {
			writer.WriteShader("shader", *shaderInfo);
		}
		if (auto* shaderInfo = program.GetShaderInfo("frag")) {
			writer.WriteShader("shader", *shaderInfo);
		}
		for (int i = 0; i < program.GetBlockCount(); ++i) {
			writer.WriteBlock("block", "member", "struct", program.GetBlock(i));
		}
		for (int i = 0; i < program.GetUniformCount(); ++i) {
			writer.WriteUniform("uniform", "sampler", program.GetUniform(i));
		}
		for (int i = 0; i < program.GetPipeInCount(); ++i) {
			writer.WritePipe("pipein", program.GetPipeIn(i));
		}
		for (int i = 0; i < program.GetPipeOutCount(); ++i) {
			writer.WritePipe("pipeout", program.GetPipeOut(i));
		}
		writer.EndWrite();
	} else if (mode == 2) {
		const char* fileName = nullptr;
		if (!fileList.empty()) {
			fileName = *fileList.begin();
		}
		ShaderXmlWriter writer(fileName);
		bool loadSuccess = writer.LoadAsProgram(program, "version", "shader", true);
		if (!loadSuccess) {
			exit(3);
		}
		writer.BeginWrite();
		writer.WriteVersion(program, "version");
		if (auto* shaderInfo = program.GetShaderInfo("vert")) {
			writer.WriteShader("shader", *shaderInfo);
		}
		if (auto* shaderInfo = program.GetShaderInfo("frag")) {
			writer.WriteShader("shader", *shaderInfo);
		}
		for (int i = 0; i < program.GetBlockCount(); ++i) {
			writer.WriteBlock("block", "member", "struct", program.GetBlock(i));
		}
		for (int i = 0; i < program.GetUniformCount(); ++i) {
			writer.WriteUniform("uniform", "sampler", program.GetUniform(i));
		}
		for (int i = 0; i < program.GetPipeInCount(); ++i) {
			writer.WritePipe("pipein", program.GetPipeIn(i));
		}
		for (int i = 0; i < program.GetPipeOutCount(); ++i) {
			writer.WritePipe("pipeout", program.GetPipeOut(i));
		}
		writer.EndWrite();
	}
	
	return 0;
}

void InitResources(TBuiltInResource& res) {
	res.maxLights = 32;
	res.maxClipPlanes = 6;
	res.maxTextureUnits = 32;
	res.maxTextureCoords = 32;
	res.maxVertexAttribs = 64;
	res.maxVertexUniformComponents = 4096;
	res.maxVaryingFloats = 64;
	res.maxVertexTextureImageUnits = 32;
	res.maxCombinedTextureImageUnits = 80;
	res.maxTextureImageUnits = 32;
	res.maxFragmentUniformComponents = 4096;
	res.maxDrawBuffers = 32;
	res.maxVertexUniformVectors = 128;
	res.maxVaryingVectors = 8;
	res.maxFragmentUniformVectors = 16;
	res.maxVertexOutputVectors = 16;
	res.maxFragmentInputVectors = 15;
	res.minProgramTexelOffset = -8;
	res.maxProgramTexelOffset = 7;
	res.maxClipDistances = 8;
	res.maxComputeWorkGroupCountX = 65535;
	res.maxComputeWorkGroupCountY = 65535;
	res.maxComputeWorkGroupCountZ = 65535;
	res.maxComputeWorkGroupSizeX = 1024;
	res.maxComputeWorkGroupSizeY = 1024;
	res.maxComputeWorkGroupSizeZ = 64;
	res.maxComputeUniformComponents = 1024;
	res.maxComputeTextureImageUnits = 16;
	res.maxComputeImageUniforms = 8;
	res.maxComputeAtomicCounters = 8;
	res.maxComputeAtomicCounterBuffers = 1;
	res.maxVaryingComponents = 60;
	res.maxVertexOutputComponents = 64;
	res.maxGeometryInputComponents = 64;
	res.maxGeometryOutputComponents = 128;
	res.maxFragmentInputComponents = 128;
	res.maxImageUnits = 8;
	res.maxCombinedImageUnitsAndFragmentOutputs = 8;
	res.maxCombinedShaderOutputResources = 8;
	res.maxImageSamples = 0;
	res.maxVertexImageUniforms = 0;
	res.maxTessControlImageUniforms = 0;
	res.maxTessEvaluationImageUniforms = 0;
	res.maxGeometryImageUniforms = 0;
	res.maxFragmentImageUniforms = 8;
	res.maxCombinedImageUniforms = 8;
	res.maxGeometryTextureImageUnits = 16;
	res.maxGeometryOutputVertices = 256;
	res.maxGeometryTotalOutputComponents = 1024;
	res.maxGeometryUniformComponents = 1024;
	res.maxGeometryVaryingComponents = 64;
	res.maxTessControlInputComponents = 128;
	res.maxTessControlOutputComponents = 128;
	res.maxTessControlTextureImageUnits = 16;
	res.maxTessControlUniformComponents = 1024;
	res.maxTessControlTotalOutputComponents = 4096;
	res.maxTessEvaluationInputComponents = 128;
	res.maxTessEvaluationOutputComponents = 128;
	res.maxTessEvaluationTextureImageUnits = 16;
	res.maxTessEvaluationUniformComponents = 1024;
	res.maxTessPatchComponents = 120;
	res.maxPatchVertices = 32;
	res.maxTessGenLevel = 64;
	res.maxViewports = 16;
	res.maxVertexAtomicCounters = 0;
	res.maxTessControlAtomicCounters = 0;
	res.maxTessEvaluationAtomicCounters = 0;
	res.maxGeometryAtomicCounters = 0;
	res.maxFragmentAtomicCounters = 8;
	res.maxCombinedAtomicCounters = 8;
	res.maxAtomicCounterBindings = 1;
	res.maxVertexAtomicCounterBuffers = 0;
	res.maxTessControlAtomicCounterBuffers = 0;
	res.maxTessEvaluationAtomicCounterBuffers = 0;
	res.maxGeometryAtomicCounterBuffers = 0;
	res.maxFragmentAtomicCounterBuffers = 1;
	res.maxCombinedAtomicCounterBuffers = 1;
	res.maxAtomicCounterBufferSize = 16384;
	res.maxTransformFeedbackBuffers = 4;
	res.maxTransformFeedbackInterleavedComponents = 64;
	res.maxCullDistances = 8;
	res.maxCombinedClipAndCullDistances = 8;
	res.maxSamples = 4;
	res.limits.nonInductiveForLoops = 1;
	res.limits.whileLoops = 1;
	res.limits.doWhileLoops = 1;
	res.limits.generalUniformIndexing = 1;
	res.limits.generalAttributeMatrixVectorIndexing = 1;
	res.limits.generalVaryingIndexing = 1;
	res.limits.generalSamplerIndexing = 1;
	res.limits.generalVariableIndexing = 1;
	res.limits.generalConstantMatrixVectorIndexing = 1;
}

std::string ReadFile(const char* name) {
	std::string fileString;

	std::ifstream fs;
	fs.open(name, std::ios::in);
	if (fs.bad()) {
		return fileString;
	}
	fs.seekg(0, std::ios::end);
	size_t length = fs.tellg();
	fs.seekg(0, std::ios::beg);
	fileString.resize(length);
	fs.read(&fileString[0], length);
	fs.close();
	return fileString;
}

std::vector<uint32_t> GenerateSpv(const glslang::TIntermediate & intermediate) {
	std::vector<uint32_t> spvCode{};
	glslang::GlslangToSpv(intermediate, spvCode);
	return std::move(spvCode);
}

std::string GetVecString(std::vector<uint32_t>& code, char separator) {
	constexpr size_t NUMBER_STRING_LENGTH = 11;
	std::string codeString{};
	if (code.empty()) {
		return codeString;
	}
	codeString.reserve(code.size() * (NUMBER_STRING_LENGTH + 1));
	char numberString[NUMBER_STRING_LENGTH]{};
	for (size_t i = 0; i < code.size(); ++i) {
		int len = snprintf(numberString, NUMBER_STRING_LENGTH, "0x%08X", code.at(i));
		if (len >= 0 && len < NUMBER_STRING_LENGTH) {
			numberString[len] = 0;
		}
		if (i > 0) {
			codeString.append(1, separator);
		}
		if (i % 8 == 0) {
			codeString.append("\n");
		}
		codeString.append(numberString);
	}
	return codeString;
}

std::string GetDimString(const glslang::TType & ttype, char separator) {
	if (ttype.isScalarOrVec1()) {
		return std::to_string(1);
	}

	if (ttype.isVector()) {
		return std::to_string(ttype.getVectorSize());
	}

	if (ttype.isMatrix()) {
		char colrow[4]{};
		snprintf(colrow, 4, "%d,%d", ttype.getMatrixCols(), ttype.getMatrixRows());
		return colrow;
	}


	std::string dimString{};
	do {
		if (!ttype.isArray()) {
			break;
		}
		auto* arraySize = ttype.getArraySizes();
		if (!arraySize) {
			break;
		}
		for (int i = 0; i < arraySize->getNumDims(); i++) {
			if (i > 0) {
				dimString.append(1, ',');
			}
			dimString.append(std::to_string(arraySize->getDimSize(i)));
		}
	} while (false);
	return dimString;
}

const char* GetDimString(glslang::TSamplerDim dim) {
	switch (dim) {
	case glslang::Esd1D: return "1D";
	case glslang::Esd2D: return "2D";
	case glslang::Esd3D: return "3D";
	case glslang::EsdCube: return "Cube";
	case glslang::EsdSubpass: return "Subpass";
	default: return nullptr;
	}
}

EShLanguage GetStageByName(const char * stageName) {
	if (strcmp(stageName, "vert") == 0)	return EShLangVertex;
	else if (strcmp(stageName, "tesc") == 0) return EShLangTessControl;
	else if (strcmp(stageName, "tese") == 0) return EShLangTessEvaluation;
	else if (strcmp(stageName, "geom") == 0) return EShLangGeometry;
	else if (strcmp(stageName, "frag") == 0) return EShLangFragment;

	return kInvalidStage;
}

const char * GetStageName(EShLanguage stage) {
	switch (stage) {
	case EShLangVertex: return "vert";
	case EShLangTessControl: return "tesc";
	case EShLangTessEvaluation: return "tese";
	case EShLangGeometry: return "geom";
	case EShLangFragment: return "frag";
	default: return nullptr;
	}
}

std::string GetStagesString(EShLanguageMask mask, char separator) {
	std::string stagesString{};
	std::vector<const char*> container{};
	if (mask & EShLangVertexMask) container.push_back("vert");
	if (mask & EShLangFragmentMask) container.push_back("frag");
	if (mask & EShLangTessControlMask) container.push_back("tesc");
	if (mask & EShLangTessEvaluationMask) container.push_back("tese");
	if (mask & EShLangGeometryMask) container.push_back("geom");
	for (size_t i = 0; i < container.size(); i++) {
		if (i > 0) {
			stagesString.append(1, separator);
		}
		stagesString.append(container.at(i));
	}
	return stagesString;
}

glslang::EShTargetClientVersion GetClientVersion(const char * verString) {
	if (0 == strcmp(verString, "1.0")) {
		return glslang::EShTargetVulkan_1_0;
	} else if (0 == strcmp(verString, "1.1")) {
		return glslang::EShTargetVulkan_1_1;
	} else if (0 == strcmp(verString, "1.2")) {
		return glslang::EShTargetVulkan_1_2;
	} else {
		return kTargetClientVersion;
	}
}

const char * GetClientVersionString(glslang::EShTargetClientVersion version) {
	switch (version) {
	case glslang::EShTargetVulkan_1_0: return "1.0";
	case glslang::EShTargetVulkan_1_1: return "1.1";
	case glslang::EShTargetVulkan_1_2: return "1.2";
	default: break;
	}
	return nullptr;
}

glslang::EShTargetLanguageVersion GetLanguageVersion(const char * verString) {
	if (0 == strcmp(verString, "1.0")) {
		return glslang::EShTargetSpv_1_0;
	} else if (0 == strcmp(verString, "1.1")) {
		return glslang::EShTargetSpv_1_1;
	} else if (0 == strcmp(verString, "1.2")) {
		return glslang::EShTargetSpv_1_2;
	} else if (0 == strcmp(verString, "1.3")) {
		return glslang::EShTargetSpv_1_3;
	} else if (0 == strcmp(verString, "1.4")) {
		return glslang::EShTargetSpv_1_4;
	} else if (0 == strcmp(verString, "1.5")) {
		return glslang::EShTargetSpv_1_5;
	} else {
		return kTargetLanguageVersion;
	}
}

const char * GetLanguageVersionString(glslang::EShTargetLanguageVersion version) {
	switch (version) {
	case glslang::EShTargetSpv_1_0: return "1.0";
	case glslang::EShTargetSpv_1_1: return "1.1";
	case glslang::EShTargetSpv_1_2: return "1.2";
	case glslang::EShTargetSpv_1_3: return "1.3";
	case glslang::EShTargetSpv_1_4: return "1.4";
	case glslang::EShTargetSpv_1_5: return "1.5";
	default: break;
	}
	return nullptr;
}

std::vector<uint32_t> GetVecFromString(const char * srcString, char separator) {
	std::vector<uint32_t> code{};
	const char* pos = srcString;
	do {
		char* endPos = nullptr;
		uint32_t value = strtoul(pos, &endPos, 16);
		code.push_back(value);
		if (!endPos || endPos == pos) {
			code.clear();
			break;
		}
		pos = strchr(endPos, separator);
		if (pos) {
			++pos;
		}
	} while (pos);
	return code;
}
