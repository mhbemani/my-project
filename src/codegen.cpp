#include "codegen.h"
#include <llvm/IR/Verifier.h>
#include <stdexcept>

using namespace llvm;

CodeGen::CodeGen() :
    context(std::make_unique<LLVMContext>()),
    module(std::make_unique<Module>("main", *context)),
    builder(std::make_unique<IRBuilder<>>(*context)) {

    // Create main function
    FunctionType* mainType = FunctionType::get(Type::getInt32Ty(*context), false);
    Function* mainFunc = Function::Create(mainType, Function::ExternalLinkage, "main", *module);
    
    // Create entry block
    BasicBlock* entry = BasicBlock::Create(*context, "entry", mainFunc);
    builder->SetInsertPoint(entry);
}

void CodeGen::generate(ProgramNode& ast) {
    for (auto& stmt : ast.statements) {
        generateStatement(stmt.get());
    }
    
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRet(ConstantInt::get(Type::getInt32Ty(*context), 0));
    }
    
    // Verify the module
    std::string error;
    raw_string_ostream os(error);
    if (verifyModule(*module, &os)) {
        throw std::runtime_error("Generated IR is invalid: " + error);
    }
}

void CodeGen::generateStatement(ASTNode* node) {
    if (auto multiVarDecl = dynamic_cast<MultiVarDeclNode*>(node)) {
        // Handle multiple variable declarations
        for (auto& decl : multiVarDecl->declarations) {
            generateVarDecl(decl.get());
        }
    } else if (auto varDecl = dynamic_cast<VarDeclNode*>(node)) {
        generateVarDecl(varDecl);
    } else if (auto assign = dynamic_cast<AssignNode*>(node)) {
        generateAssign(assign);
    } else if (auto compound = dynamic_cast<CompoundAssignNode*>(node)) { // new
        generateCompoundAssign(compound);
    } else if (auto ifElseNode = dynamic_cast<IfElseNode*>(node)) {
        // Handle if-else statement
        generateIfElse(ifElseNode); // Call the updated generateIfElse
    }
}

void CodeGen::generateVarDecl(VarDeclNode* node) {
    // if (symbols.find(node->name) != symbols.end()) {
    //     throw std::runtime_error("Redeclaration of variable: " + node->name);
    // }
    
    // Create the appropriate type based on variable type
    Type* type;
    switch (node->type) {
        case VarType::INT: type = Type::getInt32Ty(*context); break;
        case VarType::BOOL: type = Type::getInt1Ty(*context); break;
        case VarType::FLOAT: type = Type::getFloatTy(*context); break;
        case VarType::CHAR: type = Type::getInt8Ty(*context); break;
        case VarType::STRING: type = PointerType::get(Type::getInt8Ty(*context), 0); break;
        default: throw std::runtime_error("Unknown variable type");
    }
    
    AllocaInst* alloca = builder->CreateAlloca(type, nullptr, node->name);
    symbols[node->name] = alloca;
    
    if (node->value) {
        Value* val = generateValue(node->value.get(), type);
        builder->CreateStore(val, alloca);
    }
}

void CodeGen::generateAssign(AssignNode* node) {
    auto it = symbols.find(node->name);
    if (it == symbols.end()) {
        throw std::runtime_error("Assignment to undeclared variable: " + node->name);
    }
    
    AllocaInst* alloca = it->second;
    Type* expectedType = alloca->getAllocatedType();
    Value* val = generateValue(node->value.get(), expectedType);
    
    builder->CreateStore(val, alloca);
}

void CodeGen::generateCompoundAssign(CompoundAssignNode* node) {
    auto it = symbols.find(node->name);
    if (it == symbols.end()) {
        throw std::runtime_error("Compound assignment to undeclared variable: " + node->name);
    }

    AllocaInst* alloca = it->second;
    Type* type = alloca->getAllocatedType();

    // Load current value
    Value* current = builder->CreateLoad(type, alloca);

    // Evaluate right-hand side
    Value* rhs = generateValue(node->value.get(), type);

    // Perform the operation
    Value* result = nullptr;
    switch (node->op) {
        case BinaryOp::ADD:
            result = type->isFloatTy() ? builder->CreateFAdd(current, rhs)
                                       : builder->CreateAdd(current, rhs);
            break;
        case BinaryOp::SUBTRACT:
            result = type->isFloatTy() ? builder->CreateFSub(current, rhs)
                                       : builder->CreateSub(current, rhs);
            break;
        case BinaryOp::MULTIPLY:
            result = type->isFloatTy() ? builder->CreateFMul(current, rhs)
                                       : builder->CreateMul(current, rhs);
            break;
        case BinaryOp::DIVIDE:
            result = type->isFloatTy() ? builder->CreateFDiv(current, rhs)
                                       : builder->CreateSDiv(current, rhs);  // Signed integer division
            break;
        default:
            throw std::runtime_error("Unsupported compound assignment operator");
    }

    // Store result back
    builder->CreateStore(result, alloca);
}

void CodeGen::generateIfElse(IfElseNode* node) {
    Value* conditionValue = generateValue(node->condition.get(), Type::getInt1Ty(*context));
    Function* parentFunc = builder->GetInsertBlock()->getParent();
    BasicBlock* thenBlock = BasicBlock::Create(*context, "then", parentFunc);
    BasicBlock* elseBlock = node->hasElseBlock() ? BasicBlock::Create(*context, "else", parentFunc) : nullptr;
    BasicBlock* afterIfElseBlock = BasicBlock::Create(*context, "after_if_else", parentFunc);

    builder->CreateCondBr(conditionValue, thenBlock, elseBlock ? elseBlock : afterIfElseBlock);

    builder->SetInsertPoint(thenBlock);
    if (auto* block = dynamic_cast<BlockNode*>(node->then_block.get())) {
        generateBlock(block);
    } else {
        throw std::runtime_error("Expected BlockNode for then_block in IfElseNode");
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(afterIfElseBlock);
    }

    if (elseBlock) {
        builder->SetInsertPoint(elseBlock);
        if (node->isElseIf()) {
            generateIfElse(dynamic_cast<IfElseNode*>(node->else_block.get()));
            // Ensure the recursive call's block ends properly; rely on its after_if_else
        } else if (auto* block = dynamic_cast<BlockNode*>(node->else_block.get())) {
            generateBlock(block);
            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(afterIfElseBlock);
            }
        } else {
            throw std::runtime_error("Expected BlockNode or IfElseNode for else_block");
        }
        // Add a terminator if the else block itself needs one
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(afterIfElseBlock);
        }
    }

    builder->SetInsertPoint(afterIfElseBlock);
}



void CodeGen::generateBlock(BlockNode* blockNode) {
    // Iterate through all the statements in the block and generate them
    for (auto& statement : blockNode->statements) {
        generateStatement(statement.get());
    }
}

llvm::Value* CodeGen::generateValue(ASTNode* node, llvm::Type* expectedType) {
    if (auto intLit = dynamic_cast<IntLiteral*>(node)) {
        if (expectedType->isIntegerTy()) {
            return ConstantInt::get(Type::getInt32Ty(*context), intLit->value);
        }
        else if (expectedType->isFloatTy()) {
            return ConstantFP::get(Type::getFloatTy(*context), static_cast<double>(intLit->value));
        }
        throw std::runtime_error("Type mismatch: cannot convert integer to target type");
    }
    else if (auto floatLit = dynamic_cast<FloatLiteral*>(node)) {
        if (!expectedType->isFloatTy()) {
            throw std::runtime_error("Expected float type");
        }
        return ConstantFP::get(Type::getFloatTy(*context), floatLit->value);
    }
    else if (auto strLit = dynamic_cast<StrLiteral*>(node)) {
        if (!expectedType->isPointerTy()) {
            throw std::runtime_error("Expected pointer type for string");
        }
        return builder->CreateGlobalStringPtr(strLit->value);
    }
    else if (auto boolLit = dynamic_cast<BoolLiteral*>(node)) {
        if (!expectedType->isIntegerTy(1)) {
            throw std::runtime_error("Expected boolean type");
        }
        return ConstantInt::get(Type::getInt1Ty(*context), boolLit->value);
    }
    else if (auto charLit = dynamic_cast<CharLiteral*>(node)) {
        if (!expectedType->isIntegerTy(8)) {
            throw std::runtime_error("Expected char (i8) type");
        }
        return ConstantInt::get(Type::getInt8Ty(*context), charLit->value);
    }
    else if (auto binOp = dynamic_cast<BinaryOpNode*>(node)) {
        // Handle comparison and logical operators (expect i1 output)
        if (binOp->op == BinaryOp::EQUAL || binOp->op == BinaryOp::LESS_EQUAL ||
            binOp->op == BinaryOp::NOT_EQUAL || binOp->op == BinaryOp::GREATER ||
            binOp->op == BinaryOp::GREATER_EQUAL || binOp->op == BinaryOp::LESS ||
            binOp->op == BinaryOp::AND || binOp->op == BinaryOp::OR) {
            Value* left = generateValue(binOp->left.get(), binOp->op == BinaryOp::AND || binOp->op == BinaryOp::OR ? Type::getInt1Ty(*context) : Type::getInt32Ty(*context));
            Value* right = generateValue(binOp->right.get(), binOp->op == BinaryOp::AND || binOp->op == BinaryOp::OR ? Type::getInt1Ty(*context) : Type::getInt32Ty(*context));
            switch (binOp->op) {
                case BinaryOp::EQUAL: return builder->CreateICmpEQ(left, right);
                case BinaryOp::LESS_EQUAL: return builder->CreateICmpSLE(left, right);
                case BinaryOp::NOT_EQUAL: return builder->CreateICmpNE(left, right);
                case BinaryOp::GREATER: return builder->CreateICmpSGT(left, right);
                case BinaryOp::GREATER_EQUAL: return builder->CreateICmpSGE(left, right);
                case BinaryOp::LESS: return builder->CreateICmpSLT(left, right);
                case BinaryOp::AND: return builder->CreateAnd(left, right);
                case BinaryOp::OR: return builder->CreateOr(left, right);
                default: throw std::runtime_error("Unreachable");
            }
        }
        // Existing arithmetic operators
        Value* left = generateValue(binOp->left.get(), expectedType);
        Value* right = generateValue(binOp->right.get(), expectedType);
        switch (binOp->op) {
            case BinaryOp::ADD:
                return expectedType->isFloatTy() ? builder->CreateFAdd(left, right)
                                                : builder->CreateAdd(left, right);
            case BinaryOp::SUBTRACT:
                return expectedType->isFloatTy() ? builder->CreateFSub(left, right)
                                                : builder->CreateSub(left, right);
            case BinaryOp::MULTIPLY:
                return expectedType->isFloatTy() ? builder->CreateFMul(left, right)
                                                : builder->CreateMul(left, right);
            case BinaryOp::DIVIDE:
                return expectedType->isFloatTy() ? builder->CreateFDiv(left, right)
                                                : builder->CreateSDiv(left, right);
            default:
                throw std::runtime_error("Unsupported binary operator");
        }
    }
    else if (auto varRef = dynamic_cast<VarRefNode*>(node)) {
        auto it = symbols.find(varRef->name);
        if (it == symbols.end()) {
            throw std::runtime_error("Undeclared variable: " + varRef->name);
        }
        AllocaInst* alloca = it->second;
        if (alloca->getAllocatedType() != expectedType) {
            throw std::runtime_error("Type mismatch: variable " + varRef->name + " has a different type");
        }
        return builder->CreateLoad(expectedType, alloca);
    }
    throw std::runtime_error("Unsupported value type in code generation");
}

void CodeGen::dump() const {
    module->print(llvm::outs(), nullptr);
}