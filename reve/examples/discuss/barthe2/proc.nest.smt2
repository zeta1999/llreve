(set-info :origin "NTS benchmark converted to SMT-LIB2 using Eldarica (http://lara.epfl.ch/w/eldarica)")
(set-logic HORN)
(declare-fun INV_1_MAIN (Int Int Int Int Int Int) Bool)
(declare-fun INV_2_MAIN (Int Int Int Int Int Int) Bool)
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int))
         (=> (and (= (* (- 1) A) (- 1))
                  (= (* (- 1) C) (- 1))
                  (= (* (- 1) D) (- 1))
                  (= (* (- 1) F) (- 1))
                  (= B E))
             (INV_1_MAIN A B C D E F))))
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int) (G Int) (H Int) (I Int) (J Int))
         (=> (and (= (+ G 1) A)
                  (= (* 5 H) C)
                  (= (+ I 1) D)
                  (= (* 5 J) F)
                  (INV_1_MAIN G B H I E J)
                  (>= E I)
                  (>= B G))
             (INV_1_MAIN A B C D E F))))
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int) (G Int) (H Int))
         (=> (and (= (+ G 1) A)
                  (= (* 5 H) C)
                  (not (>= E D))
                  (>= B G)
                  (INV_1_MAIN G B H D E F))
             (INV_1_MAIN A B C D E F))))
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int) (G Int) (H Int))
         (=> (and (= (+ G 1) D)
                  (= (* 5 H) F)
                  (not (>= B A))
                  (>= E G)
                  (INV_1_MAIN A B C G E H))
             (INV_1_MAIN A B C D E F))))
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int) (G Int) (H Int))
         (=> (and (= (* (- 1) A) 0)
                  (= (* (- 1) D) (- 1))
                  (not (>= E G))
                  (not (>= B H))
                  (INV_1_MAIN H B C G E F))
             (INV_2_MAIN A B C D E F))))
(assert
 (not (exists ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int))
              (and (not (= A B))
                   (not (>= C D))
                   (not (>= E F))
                   (INV_2_MAIN F E A D C B)))))
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int) (G Int) (H Int))
         (=> (and (= (+ G 1) A)
                  (= (+ H G) C)
                  (not (>= E D))
                  (>= B G)
                  (INV_2_MAIN G B H D E F))
             (INV_2_MAIN A B C D E F))))
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int) (G Int) (H Int))
         (=> (and (= (+ G 1) D)
                  (= (+ H G) F)
                  (not (>= B A))
                  (>= E G)
                  (INV_2_MAIN A B C G E H))
             (INV_2_MAIN A B C D E F))))
(assert
 (forall ((A Int) (B Int) (C Int) (D Int) (E Int) (F Int) (G Int) (H Int) (I Int) (J Int) (K Int) (L Int) (M Int) (N Int) (O Int) (P Int))
         (=> (and (= (+ G 1) A)
                  (= (+ H G) C)
                  (= (+ I 1) D)
                  (= (+ J I) F)
                  (>= E I)
                  (>= B G)
                  (INV_2_MAIN G B H I E J)
                  (not (>= K L))
                  (not (>= M N))
                  (INV_1_MAIN N M O L K P))
             (INV_2_MAIN A B C D E F))))
(check-sat)
