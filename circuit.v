module comparatOR_4bit_structural(
    output G, E, L,           // A>B, A=B, A<B
    input a3, a2, a1, a0,     // Input A bits (a3 is MSB)
    input b3, b2, b1, b0      // Input B bits (b3 is MSB)
);
    // Internal wires fOR bit-wise equality (XNOR)
    wire x3, x2, x1, x0;
    
    // Internal wires fOR inverted inputs
    wire na3, na2, na1, na0;
    wire nb3, nb2, nb1, nb0;
    
    // Internal wires fOR product terms (Greater Than)
    wire g_term0, g_term1, g_term2, g_term3;
    
    // Internal wires fOR product terms (Less Than)
    wire l_term0, l_term1, l_term2, l_term3;

    // 1. Invert all inputs fOR comparison logic
    NOT (na3, a3);
    NOT (na2, a2); 
    NOT (na1, a1); 
    NOT (na0, a0);
    NOT (nb3, b3);   
    NOT (nb2, b2);
    NOT (nb1, b1);
    NOT (nb0, b0);
    // 2. Equality check fOR each bit (A == B if x[i] is 1)
    XNOR (x3, a3, b3);
    XNOR (x2, a2, b2);
    XNOR (x1, a1, b1);
    XNOR (x0, a0, b0);

    // 3. Final Equality Output (A = B)
    // All bits must be equal
    AND (E, x3, x2, x1, x0);

    // 4. Greater Than Logic (A > B)
    // FORmula: (a3 > b3) | (x3 & a2 > b2) | (x3 & x2 & a1 > b1) | (x3 & x2 & x1 & a0 > b0)
    AND (g_term3, a3, nb3);
    AND (g_term2, x3, a2, nb2);
    AND (g_term1, x3, x2, a1, nb1);
    AND (g_term0, x3, x2, x1, a0, nb0);
    OR  (G, g_term3, g_term2, g_term1, g_term0);

    // 5. Less Than Logic (A < B)
    // FORmula: (b3 > a3) | (x3 & b2 > a2) | (x3 & x2 & b1 > a1) | (x3 & x2 & x1 & b0 > a0)
    AND (l_term3, na3, b3);
    AND (l_term2, x3, na2, b2);
    AND (l_term1, x3, x2, na1, b1);
    AND (l_term0, x3, x2, x1, na0, b0);
    OR  (L, l_term3, l_term2, l_term1, l_term0);

endmodule