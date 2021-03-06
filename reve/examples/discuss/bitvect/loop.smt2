;;; i1 j1 n1 i2 j2
(set-logic HORN)
;; (define-fun INV_42_MAIN ((A (_ BitVec 32)) (B (_ BitVec 32)) (C (_ BitVec 32)) (D (_ BitVec 32)) (E (_ BitVec 32))) Bool
;;   (and (= (bvadd A D) C) (= B E) (bvsge A #x00000000) (bvsge B #x00000000)))
(declare-fun INV_42_MAIN ((_ BitVec 32) (_ BitVec 32) (_ BitVec 32) (_ BitVec 32) (_ BitVec 32)) Bool)
;; (assert (forall ((A (_ BitVec 32)) (B (_ BitVec 32)) (C (_ BitVec 32)) (D (_ BitVec 32)) (E (_ BitVec 32)))
;;                 (=> (INV_42_MAIN A B C D E)
;;                     (INV_42_MAIN_ A B C D E))))
(assert (forall ((n (_ BitVec 32)))
                (=> (bvsge n #x00000000)
                    (bvslt n #x000000ff)
                    (INV_42_MAIN #x00000000 #x00000000 n n #x00000000))))
(assert (forall ((n1 (_ BitVec 32)) (i1 (_ BitVec 32)) (j1 (_ BitVec 32)) (i2 (_ BitVec 32)) (j2 (_ BitVec 32)))
                (=> (and (bvsge i2 #x00000000)
                         (bvsge n1 i1)
                         (INV_42_MAIN i1 j1 n1 i2 j2))
                    (INV_42_MAIN (bvadd i1 #x00000001) (bvadd j1 #x00000001) n1 (bvsub i2 #x00000001) (bvadd j2 #x00000001)))))
(assert
 (not (exists ((j1 (_ BitVec 32)) (j2 (_ BitVec 32)) (i2 (_ BitVec 32)) (n1 (_ BitVec 32)) (i1 (_ BitVec 32)))
              (and (not (= j1 j2))
                   (not (bvsge i2 #x00000000))
                   (not (bvsge n1 i1))
                   (INV_42_MAIN i1 j1 n1 i2 j2)))))
(assert
 (not (exists ((i2 (_ BitVec 32)) (n1 (_ BitVec 32)) (i1 (_ BitVec 32)) (j1 (_ BitVec 32)) (j2 (_ BitVec 32)))
              (and (bvsge i2 #x00000000)
                   (not (bvsge n1 i1))
                   (INV_42_MAIN i1 j1 n1 i2 j2)))))
(assert
 (not (exists ((i2 (_ BitVec 32)) (n1 (_ BitVec 32)) (i1 (_ BitVec 32)) (j1 (_ BitVec 32)) (j2 (_ BitVec 32)))
              (and (not (bvsge i2 #x00000000))
                   (bvsge n1 i1)
                   (INV_42_MAIN i1 j1 n1 i2 j2)))))
(check-sat)
(get-model)
