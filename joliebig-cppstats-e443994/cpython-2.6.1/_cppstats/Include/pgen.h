#if !defined(Py_PGEN_H)
#define Py_PGEN_H
#if defined(__cplusplus)
extern "C" {
#endif
/* Parser generator interface */
extern grammar *meta_grammar(void);
struct _node;
extern grammar *pgen(struct _node *);
#if defined(__cplusplus)
}
#endif
#endif /* !Py_PGEN_H */
