/**
 * @file codegen.c
 * @brief Implementation of the IFJcode2026 code generator
 * @author Tadeáš Čupr <xcuprta00@stud.fit.vut.cz>
 * @date 04.11.2026
 */

#include "codegen.h"
#include "ast.h"
#include "return.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_TMP_VARS 16

typedef struct {
    FILE *out;
    int label_counter;
    int tmp_counter;
} CodeGenContext;

static const char *binop_instr_table[] = {
    [OP_SUB] = "SUB",
    [OP_MUL] = "MUL",
    [OP_EQ]  = "EQ",
    [OP_LT]  = "LT",
    [OP_GT]  = "GT",
    [OP_AND] = "AND",
    [OP_OR]  = "OR",
};

static const char *unop_instr_table[] = {
    [OP_NOT] = "NOT",
};

#define BINOP_TABLE_SIZE (sizeof(binop_instr_table) / sizeof(binop_instr_table[0]))
#define UNOP_TABLE_SIZE (sizeof(unop_instr_table) / sizeof(unop_instr_table[0]))

static void generate_prolog(CodeGenContext *ctx) {
    fprintf(ctx->out, ".IFJcode26\n");
    fprintf(ctx->out, "CREATEFRAME\n");
    fprintf(ctx->out, "PUSHFRAME\n");
    fprintf(ctx->out, "CALL @main\n");
    fprintf(ctx->out, "EXIT int@0\n\n");
}

static void print_escaped_string(CodeGenContext *ctx, const char *str) {
    fprintf(ctx->out, "string@");
    if (str == NULL) return;

    for (size_t i = 0; str[i] != '\0'; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c<= 32 || c == 35 || c == 92) {
            fprintf(ctx->out, "\\%03d", c);
        } else {
            fputc(c, ctx->out);
        }
    }
}
static char *new_tmp_var(CodeGenContext *ctx) {
    char *buf = malloc(32);
    if (buf == NULL) return NULL;
    int id = ctx->tmp_counter % MAX_TMP_VARS;
    ctx->tmp_counter++;
    snprintf(buf, 32, "$tmp_%d", id);
    return buf;
}

static void emit_var_defs(CodeGenContext *ctx, ASTNode *node) {
    while (node != NULL) {
        switch (node->node_type) {
            case NODE_VARIABLE_DEF:
                fprintf(ctx->out, "DEFVAR LF@%s\n", node->data.variable_def.name);
                if (node->data.variable_def.expr != NULL) {
                    emit_var_defs(ctx, node->data.variable_def.expr);
                }
                break;
            case NODE_BLOCK:
                emit_var_defs(ctx, node->data.block.statements);
                break;
            case NODE_IF:
                emit_var_defs(ctx, node->data.if_node.then_branch);
                emit_var_defs(ctx, node->data.if_node.else_branch);
                break;
            case NODE_WHILE:
                emit_var_defs(ctx, node->data.while_node.body);
                break;
            default:
                break;
        }
        node = node->next;
    }
}

static void generate_expression(CodeGenContext *ctx, ASTNode *expr, const char *dest_var) {
    if (expr == NULL) return;

    switch (expr->node_type) {
        case NODE_LITERAL:
            if (dest_var == NULL) break;
            switch (expr->data_type) {
                case INT:
                    fprintf(ctx->out, "MOVE LF@%s int@%lld\n", dest_var, expr->data.literal.int_val);
                    break;
                case DOUBLE:
                    fprintf(ctx->out, "MOVE LF@%s float@%a\n", dest_var, expr->data.literal.dbl_val);
                    break;
                case STRING:
                    fprintf(ctx->out, "MOVE LF@%s", dest_var);
                    print_escaped_string(ctx, expr->data.literal.string);
                    fprintf(ctx->out, "\n");
                    break;
                case BOOLEAN:
                    fprintf(ctx->out, "MOVE LF@%s bool@%s\n", dest_var, expr->data.literal.bool_val ? "true" : "false");
                    break;
                case UNIT:
                    fprintf(ctx->out, "MOVE LF@%s nil@nil\n", dest_var);
                    break;
                default:
                    break;                                        
            }
            break;

        case NODE_VARIABLE_REF:
            if (dest_var != NULL) {
                fprintf(ctx->out, "MOVE LF@%s LF@%s\n", dest_var, expr->data.variable_ref.name);
            }
            break;

        case NODE_UNARY_OP: {
            char *op_tmp = new_tmp_var(ctx);
            generate_expression(ctx, expr->data.unary_op.operand, op_tmp);
            
            if (dest_var != NULL) {
                ASTOperator op = expr->data.unary_op.op;
                if (op == OP_NEG) {
                    const char *zero = (expr->data_type == DOUBLE) ? "float@0x0p+0" : "int@0";
                    fprintf(ctx->out, "SUB LF@%s %s LF@%s\n", dest_var, zero, op_tmp);
                } else if (op < UNOP_TABLE_SIZE && unop_instr_table[op] != NULL) {
                    fprintf(ctx->out, "%s LF@%s LF@%s\n", unop_instr_table[op], dest_var, op_tmp);
                }
            }
            free(op_tmp);
            break;
        }

        case NODE_BINARY_OP: {
            char *left_tmp = new_tmp_var(ctx);
            char *right_tmp = new_tmp_var(ctx);
            
            generate_expression(ctx, expr->data.binary_op.left, left_tmp);
            generate_expression(ctx, expr->data.binary_op.right, right_tmp);

            if (expr->data_type == DOUBLE) {
                if (expr->data.binary_op.left->data_type == INT) {
                    char *conv_tmp = new_tmp_var(ctx);
                    fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", conv_tmp, left_tmp);
                    free(left_tmp);
                    left_tmp = conv_tmp;
                }
            
                if (expr->data.binary_op.right->data_type == INT) {
                    char *conv_tmp = new_tmp_var(ctx);
                    fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", conv_tmp, right_tmp);
                    free(right_tmp);
                    right_tmp = conv_tmp;
                }
            }

            if (dest_var != NULL) {
                ASTOperator op = expr->data.binary_op.op;
                const char *inst;

                switch (op) {
                    case OP_ADD:
                        inst = (expr->data_type == STRING) ? "CONCAT" : "ADD";
                        fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", inst, dest_var, left_tmp, right_tmp);
                        break;
                    case OP_DIV:
                        inst = (expr->data_type == INT) ? "IDIV" : "DIV";
                        fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", inst, dest_var, left_tmp, right_tmp);
                        break;
                    case OP_NEQ:
                        fprintf(ctx->out, "EQ LF@%s LF@%s LF@%s\n", dest_var, left_tmp, right_tmp);
                        fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
                        break;
                    case OP_LTE:
                        fprintf(ctx->out, "GT LF@%s LF@%s LF@%s\n", dest_var, left_tmp, right_tmp);
                        fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
                        break;
                    case OP_GTE:
                        fprintf(ctx->out, "LT LF@%s LF@%s LF@%s\n", dest_var, left_tmp, right_tmp);
                        fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
                        break;
                    default:
                        if (op < BINOP_TABLE_SIZE && binop_instr_table[op] != NULL) {
                            fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", binop_instr_table[op], dest_var, left_tmp, right_tmp);
                        }
                        break;
                }
            }

            free(left_tmp);
            free(right_tmp);
            break;
        }

        case NODE_FUNCTION_CALL: {
            ASTNode *arg = expr->data.function_call.args;
            while (arg != NULL) {
                char *arg_tmp = new_tmp_var(ctx);
                generate_expression(ctx, arg, arg_tmp);
                fprintf(ctx->out, "PUSHS LF@%s\n", arg_tmp);
                free(arg_tmp);
                arg = arg->next;
            }
            fprintf(ctx->out, "CALL $%s\n", expr->data.function_call.name);

            if (dest_var != NULL) {
                fprintf(ctx->out, "POPS LF@%s\n", dest_var);
            } else {
                char *dummy = new_tmp_var(ctx);
                fprintf(ctx->out, "POPS LF@%s\n", dummy);
                free(dummy);
            }
            break;
        }

        default:
            break;
    }
}

static void generate_statement(CodeGenContext *ctx, ASTNode *statement) {
    if (statement == NULL) return;
    ctx->tmp_counter = 0;

    switch (statement->node_type) {
        case NODE_VARIABLE_DEF:
            generate_expression(ctx, statement->data.variable_def.expr, statement->data.variable_def.name);
            break;
        
        case NODE_ASSIGN:
            generate_expression(ctx, statement->data.assign.right, statement->data.assign.left);
            break;

        case NODE_BLOCK: {
            ASTNode *s = statement->data.block.statements;
            while (s != NULL) {
                generate_statement(ctx, s);
                s = s->next;
            }
            break;
        }

/* AI Model used: Gemini 
Prompt:
        case NODE_BLOCK:
            ASTNode *s = statement->data.block.statements;
            while (s != NULL) {
                generate_statement(ctx, s);
                s = s->next;
            }
            break;

src/codegen.c:214:13: error: label followed by a declaration is a C23 extension [-Werror,-Wc23-extensions]
  214 |             ASTNode *s = statement->data.block.statements;


Output:
In C standards prior to C23, a case label must be followed by a statement, not a variable declaration. Wrap the case body in curly braces {} to create a compound statement (new scope):

C
        case NODE_BLOCK: {
            ASTNode *s = statement->data.block.statements;
            while (s != NULL) {
                generate_statement(ctx, s);
                s = s->next;
            }
            break;
        }
Alternatively, place an empty statement (;) right after case NODE_BLOCK:, though adding {} is cleaner and prevents variable scope bleed across cases.

Note: wow I'm stupid, this was staring me in the face
*/

        case NODE_FUNCTION_CALL:
            generate_expression(ctx, statement, NULL);
            break;

        case NODE_RETURN:
            if (statement->data.return_node.value != NULL) {
                char *ret_tmp = new_tmp_var(ctx);
                generate_expression(ctx, statement->data.return_node.value, ret_tmp);
                fprintf(ctx->out, "PUSHS LF@%s\n", ret_tmp);
                free(ret_tmp);
            }
            fprintf(ctx->out, "POPFRAME\n");
            fprintf(ctx->out, "RETURN\n");
            break;        

        case NODE_IF: {
            int id = ctx->label_counter++;
            char *cond_tmp = new_tmp_var(ctx);

            generate_expression(ctx, statement->data.if_node.condition, cond_tmp);

            fprintf(ctx->out, "JUMPIFEQ $else_%d LF@%s bool@false\n", id, cond_tmp);
            free(cond_tmp);

            generate_statement(ctx, statement->data.if_node.then_branch);
            fprintf(ctx->out, "JUMP $if_end_%d\n", id);

            fprintf(ctx->out, "LABEL $else_%d\n", id);
            if (statement->data.if_node.else_branch != NULL) {
                generate_statemen(ctx, statement->data.if_node.else_branch);
            }

            fprintf(ctx->out, "LABEL $if_end_%d\n", id);
            break;
        }

        case NODE_WHILE: {
            int id = ctx->label_counter++;
            char *cond_tmp = new_tmp_var(ctx);

            fprintf(ctx->out, "LABEL $while_start_%d\n", id);

            generate_expression(ctx, statement->data.while_node.condition, cond_tmp);

            fprintf(ctx->out, "JUMPIFEQ $while_end_%d LF@%s bool@false\n", id, cond_tmp);
            free(cond_tmp);

            generate_statement(ctx, statement->data.while_node.body);
            fprintf(ctx->out, "JUMP #while_start_%d\n", id);

            fprintf(ctx->out, "LABEL $while_end_%d\n", id);
            break;
        }

        default:
            break;
    }
}

static void generate_function(CodeGenContext *ctx, ASTNode *fn_node) {
    if (fn_node == NULL || fn_node->node_type != NODE_FUNCTION_DEF) {
        return;
    }

    ASTFunctionDefNodeData *fn = &fn_node->data.function_def;

    if (fn->is_main) {
        fprintf(ctx->out, "LAVEL @main\n");
    } else {
        fprintf(ctx->out, "LABEL $%s\n", fn->name);
    }

    fprintf(ctx->out, "PUSHFRAME\n");

    if (fn->param_count > 0) {
        for (size_t i = fn->param_count; i > 0; i--) {
            size_t idx = i - 1;
            fprintf(ctx->out, "DEFVAR LF@%s\n", fn->params[idx].name);
            fprintf(ctx->out, "POPS LF@%s\n", fn->params[idx].name);
        }
    }

    for (int i = 0; i < MAX_TMP_VARS; i++) {
        fprintf(ctx->out, "DEFVAR LF@$tmp_%d\n", i);
    }

    if (fn->body != NULL) {
        emit_var_defs(ctx, fn->body);

        ASTNode *statement = (fn->body->node_type == NODE_BLOCK) ? fn->body->data.block.statements : fn->body;
        while(statement != NULL) {
            generate_statement(ctx, statement);
            statement = statement->next;
        }
    }

    fprintf(ctx->out, "POPFRAME\n");
    fprintf(ctx->out, "RETURN\n\n");
}

ReturnCodes generate_code(ASTNode *program, FILE *out) {
    if (program == NULL || out == NULL || program->node_type != NODE_PROGRAM) {
        return ERR_INTERNAL;
    }

    CodeGenContext ctx = {
        .out = out,
        .label_counter = 0,
        .tmp_counter = 0,
    };

    generate_prolog(&ctx);

    ASTNode *function = program->data.program.functions;
    while (function != NULL) {
        generate_function(&ctx, function);
        function = function->next;
    }
    
    return RETURN_SUCCESS;
}
