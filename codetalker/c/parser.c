
#include "parser.h"
#include "stdlib.h"
#include <string.h>
#include "stdio.h"

#define LOG pind();printf
#define LOG //[log]

/** grammar storage and loading **/

struct Grammar* grammars = 0;
int numgrammars = 0;
int gsize = 0;

int store_grammar(struct Grammar gram) {
    int i;
    if (gsize == numgrammars) {
        struct Grammar* oldgrammars = grammars;
        grammars = (struct Grammar*)malloc(sizeof(struct Grammar)*(gsize + 5));
        for (i=0;i<numgrammars;i++) {
            grammars[i] = oldgrammars[i];
        }
        if (numgrammars != 0) {
            free(oldgrammars);
        }
        gsize += 5;
    }
    grammars[numgrammars] = gram;
    numgrammars += 1;
    return numgrammars-1;
}

struct Grammar* load_grammar(int gid) {
    return &grammars[gid];
};

void free_grammars() {
    free(grammars);
}

/** @end grammar storage and loading **/

/** parsing then? **/

struct cParseNode* parse_rule(unsigned int rule, struct Grammar* grammar, struct TokenStream* tokens, struct Error* error);

struct cParseNode* _get_parse_tree(int start, struct Grammar* gram, struct TokenStream* tokens, struct Error* error) {
    return parse_rule(start, gram, tokens, error);
}

struct cParseNode* _new_parsenode(unsigned int rule) {
    struct cParseNode* node = (struct cParseNode*)malloc(sizeof(struct cParseNode));
    node->rule = rule;
    node->next = NULL;
    node->prev = NULL;
    node->child = NULL;
    node->token = NULL;
    node->type = NNODE;
    return node;
}

int indent = 0;
int IND = 4;

void pind(void) {
    int i;
    for (i=0;i<indent;i++) {
        printf(" ");
    }
}

/**
// indent = []

def log(*a):pass
def log_(*a):
    strs = []
    for e in a:strs.append(str(e))
    print '  |'*len(indent), ' '.join(strs)

**/
struct cParseNode* parse_children(unsigned int rule, struct RuleOption* option, struct Grammar* grammar, struct TokenStream* tokens, struct Error* error);
struct cParseNode* append_nodes(struct cParseNode* one, struct cParseNode* two);
struct cParseNode* check_special(unsigned int rule, struct RuleSpecial special, struct cParseNode* current, struct Grammar* grammar, struct TokenStream* tokens,  struct Error* error);

struct cParseNode* parse_rule(unsigned int rule, struct Grammar* grammar, struct TokenStream* tokens, struct Error* error) {
    struct cParseNode* node = _new_parsenode(rule);
    struct cParseNode* tmp;
    int i;
    LOG("parsing rule %d (token at %d)\n", rule, tokens->at);
    // log('parsing rule', rule)
    indent+=IND;;
    for (i=0; i < grammar->rules.rules[rule].num; i++) {
        // log('child rule:', i)
        tmp = parse_children(rule, &(grammar->rules.rules[rule].options[i]), grammar, tokens, error);
        if (tmp != NULL) {
            LOG("CHild success! %d\n", i);
            // log('child success!', i)
            node->child = tmp;
            indent-=IND;
            return node;
        }
    }
    indent-=IND;
    return NULL;
}

// clean
struct cParseNode* parse_children(unsigned int rule, struct RuleOption* option, struct Grammar* grammar, struct TokenStream* tokens, struct Error* error) {
    LOG("parsing children of %d (token at %d)\n", rule, tokens->at);
    struct cParseNode* current = NULL;
    unsigned int i = 0, m = 0;
    unsigned int at = 0;
    struct cParseNode* tmp = NULL;
    struct RuleItem* item = NULL;
    int ignore;
    indent+=IND;
    for (i=0;i<option->num;i++) {
        item = &option->items[i];
        while (tokens->at < tokens->num) {
            ignore = 0;
            for (m=0;m<grammar->ignore.num;m++) {
                if (tokens->tokens[tokens->at].which == grammar->ignore.tokens[m]) {
                    ignore = 1;
                    break;
                }
            }
            if (ignore == 0) {
                break;
            }
            LOG("ignoring white\n");
            // log('ignoring white')
            tmp = _new_parsenode(rule);
            tmp->token = &tokens->tokens[tokens->at];
            tmp->type = NTOKEN;
            current = append_nodes(current, tmp);
            LOG("inc token %d %d\n", tokens->at, tokens->at+1);
            tokens->at += 1;
        }
        if (tokens->at < tokens->num) {
            LOG("At token %d '%s'\n", tokens->at, tokens->tokens[tokens->at].value);
        }
        if (item->type == RULE) {
            LOG(">RULE\n");
            /**
            if (0 && tokens->at >= tokens->num) { // disabling
                error->at = tokens->at;
                error->reason = 1;
                error->token = NULL;
                error->text = "ran out";
                // error[1] = ['ran out', rule, i, item->value.which];
                // log('not enough tokens')
                indent-=IND;
                return NULL;
            }
            **/
            at = tokens->at;
            tmp = parse_rule(item->value.which, grammar, tokens, error);
            if (tmp == NULL) {
                tokens->at = at;
                if (tokens->at >= error->at) {
                    error->at = tokens->at;
                    error->reason = 2;
                    error->token = &tokens->tokens[tokens->at];
                    error->text = "rule failed";
                }
                indent-=IND;
                return NULL;
            }
            current = append_nodes(current, tmp);
            continue;
        } else if (item->type == TOKEN) {
            LOG(">TOKEN\n");
            if (tokens->at >= tokens->num) {
                if (item->value.which == tokens->eof) {
                    // log('EOF -- passing')
                    LOG("EOF -- passing\n");
                    tmp = _new_parsenode(rule);
                    tmp->token = (struct Token*)malloc(sizeof(struct Token));
                    tmp->token->value = NULL;
                    tmp->token->which = tokens->eof;
                    tmp->token->lineno = -1;
                    tmp->token->charno = -1;
                    tmp->type = NTOKEN;
                    current = append_nodes(current, tmp);
                    continue;
                }
                LOG("no more tokens\n");
                error->at = tokens->at;
                error->reason = 1;
                error->token = NULL;
                error->text = "ran out";
                // log('not enough tokens')
                indent-=IND;
                return NULL;
            }
            // log('token... [looking for', item->value.which, '] got', tokens->tokens[tokens->at].which)
            if (tokens->tokens[tokens->at].which == item->value.which) {
                LOG("got token! %d\n", item->value.which);
                tmp = _new_parsenode(rule);
                tmp->token = &tokens->tokens[tokens->at];
                tmp->type = NTOKEN;
                current = append_nodes(current, tmp);
                LOG("inc token %d %d\n", tokens->at, tokens->at+1);
                tokens->at += 1;
                continue;
            } else {
                if (tokens->at > error->at) {
                    error->at = tokens->at;
                    error->reason = 3;
                    error->token = &tokens->tokens[tokens->at];
                    error->text = "token failed";
                    error->wanted = option->items[i].value.which;
                }
                LOG("token failed (wanted %d, got %d)\n",
                        item->value.which, tokens->tokens[tokens->at].which);
                // log('failed token')
                indent-=IND;
                return NULL;
            }
        } else if (item->type == LITERAL) {
            LOG(">LITERAL\n");
            if (tokens->at >= tokens->num) {
                error->at = tokens->at;
                error->reason = 1;
                error->token = NULL;
                error->text = "ran out";
                // log('not enough tokens')
                indent-=IND;
                return NULL;
            }
            // log('looking for literal', item->value.text)
            if (strcmp(item->value.text, tokens->tokens[tokens->at].value) == 0) {
                LOG("got literal!\n");
                tmp = _new_parsenode(rule);
                tmp->token = &tokens->tokens[tokens->at];
                tmp->type = NTOKEN;
                current = append_nodes(current, tmp);
                LOG("inc token %d %d\n", tokens->at, tokens->at+1);
                tokens->at += 1;
                continue;
                // log('success!!')
            } else {
                if (tokens->at > error->at) {
                    error->at = tokens->at;
                    error->reason = 4;
                    error->token = &tokens->tokens[tokens->at];
                    error->text = item->value.text;
                }
                LOG("failed....literally: %s\n", item->value.text);
                // log('failed...literally', tokens->tokens[tokens->at].value)
                indent-=IND;
                return NULL;
            }
        } else if (item->type == SPECIAL) {
            LOG(">SPECIAL\n");
            tmp = check_special(rule, item->value.special, current, grammar, tokens, error);
            if (tmp == NULL) {
                indent-=IND;
                return NULL;
            }
            current = tmp;
        }
    }
    indent-=IND;
    return current;
}

struct cParseNode* check_special(unsigned int rule, struct RuleSpecial special, struct cParseNode* current, struct Grammar* grammar, struct TokenStream* tokens, struct Error* error) {
    struct cParseNode* tmp;
    int at, i;
    // log('special')
    indent+=IND;
    if (special.type == STAR) {
        // log('star!')
        while (tokens->at < tokens->num) {
            at = tokens->at;
            tmp = parse_children(rule, special.option, grammar, tokens, error);
            if (tmp == NULL) {
                tokens->at = at;
                break;
            }
            current = append_nodes(current, tmp);
        }
        // log('awesome star')
        indent-=IND;
        return current;
    } else if (special.type == PLUS) {
        // log('plus!')
        at = tokens->at;
        tmp = parse_children(rule, special.option, grammar, tokens, error);
        if (tmp == NULL) {
            tokens->at = at;
            // log('failed plus')
            indent-=IND;
            return NULL;
        }
        current = append_nodes(current, tmp);
        while (tokens->at < tokens->num) {
            at = tokens->at;
            tmp = parse_children(rule, special.option, grammar, tokens, error);
            if (tmp == NULL) {
                tokens->at = at;
                break;
            }
            current = append_nodes(current, tmp);
        }
        // log('good plus')
        indent-=IND;
        return current;
    } else if (special.type == OR) {
        // log('or!')
        at = tokens->at;
        for (i=0;i<special.option->num;i++) {
            tmp = parse_children(rule, special.option->items[i].value.special.option, grammar, tokens, error);
            if (tmp != NULL) {
                // log('got or...')
                current = append_nodes(current, tmp);
                indent-=IND;
                return current;
            }
        }
        // log('fail or')
        indent-=IND;
        return NULL;
    } else if (special.type == QUESTION) {
        // log('?maybe')
        at = tokens->at;
        tmp = parse_children(rule, special.option, grammar, tokens, error);
        if (tmp == NULL) {
            // log('not taking it')
            indent-=IND;
            return current;
        }
        current = append_nodes(current, tmp);
        // log('got maybe')
        indent-=IND;
        return current;
    } else {
        // print 'unknown special type:', special.type;
        indent-=IND;
        return NULL;
    }
    // log('umm shouldnt happen')
    indent-=IND;
    return NULL;
}

struct cParseNode* append_nodes(struct cParseNode* one, struct cParseNode* two) {
    if (one == NULL) {
        return two;
    }
    struct cParseNode* tmp = two;
    while (tmp->prev != NULL) {
        tmp = tmp->prev;
    }
    one->next = tmp;
    tmp->prev = one;
    return two;
}
