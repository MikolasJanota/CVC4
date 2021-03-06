(set-logic AUFLIA)
(set-info :source | Simple list theorem |)
(set-info :smt-lib-version 2.6)
(set-info :category "crafted")
(set-info :status unsat)
(declare-sort List 0)
(declare-sort Elem 0)
(declare-fun cons (Elem List) List)
(declare-fun nil () List)
(declare-fun len (List) Int)
(assert (= (len nil) 0))
(assert (forall ((?x Elem) (?y List)) (= (len (cons ?x ?y)) (+ (len ?y) 1))))
(declare-fun append (List List) List)
(assert (forall ((?y List)) (= (append nil ?y) ?y)))
(assert (forall ((?x Elem) (?y1 List) (?y2 List)) (= (append (cons ?x ?y1) ?y2) (cons ?x (append ?y1 ?y2)))))
(declare-fun x () Elem)
(declare-fun y () List)
(assert (not (= (append (cons x nil) y) (cons x y))))
(check-sat)
(exit)
