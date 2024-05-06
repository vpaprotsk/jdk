import javax.management.RuntimeErrorException;

class HelloString {
    public static void main(String[] args) {
        String haystack;
        String needle;

        if (true) {
            for (Encoding ae : Encoding.values())
            //Encoding ae = Encoding.UU;
                (new HelloString(ae))
                    .benchmark();
                    // .test1()
                    // .test2();
        } else if (false) {
            needle = "1234567890";
            needle = needle + needle + needle + needle + needle + "1";
            haystack = "Hi Hello, "+needle+"World!1234123456789012345678";
        } else if (false) {
            // [acb][adb]
            haystack = "aaacbb";
            needle = "acb";
            System.out.println(haystack.indexOf2(needle, 1));
        } else if (true) {
            HelloString t = new HelloString(Encoding.UU);
            needle = t.newNeedle(3, -1);
            haystack = t.newHaystack(31, needle, 2);
            System.out.println(haystack.indexOf2(needle, 1));
        }
    }

    HelloString test0() { // Test 'trivial cases'
        // Need to disable checks in String.java
        // if (0==needle_len) return haystack_off;
        if (3 != "Hello".indexOf2("", 3)) {System.out.println("FAILED: if (0==needle_len) return haystack_off");}
        //if (0==haystack_len) return -1;
        if (-1 != "".indexOf2("Hello", 3)) {System.out.println("FAILED: if (0==haystack_len) return -1");}
        //if (needle_len>haystack_len) return -1;
        if (-1 != "Hello".indexOf2("HelloWorld", 3)) {System.out.println("FAILED: if (needle_len>haystack_len) return -1");}
        return this;
    }

    HelloString test1() { // Test expected to find
        int scope = 32*2+16+8;
        for (int nSize = 3; nSize<scope; nSize++) {
            String needle = newNeedle(nSize, -1);
            for (int hSize = nSize; hSize<scope; hSize++) {
                for (int i = 1; i<hSize-nSize; i++) {
                    //if (hSize-(nSize+i)<32) continue; //Not implemented
                    System.out.println("("+ae.name()+") Trying needle["+nSize+"] in haystack["+hSize+"] at offset["+i+"]");
                    String haystack = newHaystack(hSize, needle, i);
                    int found = haystack.indexOf2(needle, 1);
                    if (i != found) {
                        System.out.println("    FAILED: " + found + " " + haystack + "["+needle+"]");
                    }
                }
            }
        }
        return this;
    }

    HelloString test2() { // Test needle with one mismatched character
        int scope = 32*2+16+8;
        for (int nSize = 3; nSize<scope; nSize++) {
            for (int hSize = nSize; hSize<scope; hSize++) {
                String needle = newNeedle(nSize, -1);
                for (int badPosition = 1; badPosition < nSize-1; badPosition+=1) {
                    String badNeedle = newNeedle(nSize, badPosition);
                    for (int i = 1; i<hSize-nSize; i++) {
                        //if (hSize-(nSize+i)<16) continue; //Not implemented
                        System.out.println("("+ae.name()+") Trying needle["+nSize+"]["+badPosition+"] in haystack["+hSize+"] at offset["+i+"]");
                        String haystack = newHaystack(hSize, needle, i);
                        int found = haystack.indexOf2(badNeedle, 1);
                        if (-1 != found) {
                            System.out.println("    FAILED: False " + found + " " + haystack + "["+needle+"]["+badNeedle+"]");
                        }
                    }
                }
            }
        }
        return this;
    }

    final char variants = 16;
    HelloString benchmark() {
        int sizes[] = {
            2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 
            18, 20, 25, 30, 35, 40, 50, 60, 70, 80, 90, 100, 
            //150, 200, 400, 800, 1600, 3200
        };

        final boolean check = false;
        String needles[][] = new String[sizes.length][variants];
        String badNeedles[][][] = new String[sizes.length][variants][];
        String haystacks[][][][] = new String[sizes.length][variants][sizes.length][];

        for (char variant = 0; variant<variants; variant++){
            // int nSizeI = 0, hSizeI = 0, bPosI = 0;
            for (int nSizeI = 0; nSizeI < sizes.length; nSizeI++) {
                int nSize = sizes[nSizeI];
                String needle = newNeedle(nSize, -1);
                needles[nSizeI][variant] = needle;
                for (int hSizeI = 0; hSizeI< sizes.length; hSizeI++) {
                    int hSize = sizes[hSizeI];
                    if (nSize>=hSize) continue;
                    haystacks[nSizeI][variant][hSizeI] = new String[hSize-nSize-1];
                    for (int i = 1; i<hSize-nSize; i++) {
                        // if (i+nSize>hSize) continue;
                        // String haystack = newHaystack(hSize, needle, i, variant);
                        haystacks[nSizeI][variant][hSizeI][i-1] = newHaystack(hSize, needle, i, variant);
                        // System.out.println("Haystack "+nSize+" "+hSize+" "+i);
                        badNeedles[nSizeI][variant] = new String[nSize-2];
                        for (int badPosition = 1; badPosition < nSize-1; badPosition+=1) {
                            // String badNeedle = newNeedle(nSize, badPosition, variant);
                            badNeedles[nSizeI][variant][badPosition-1] = newNeedle(nSize, badPosition, variant);
                            if (check) {
                                int found = haystacks[nSizeI][variant][hSizeI][badPosition-1].indexOf2(badNeedles[nSizeI][variant][badPosition-1], 1);
                                if (-1 != found) {
                                    System.out.println("    FAILED: False " + found + " " + haystacks[nSizeI][variant][hSizeI][badPosition-1] + "["+needle+"]["+badNeedles[nSizeI][variant][badPosition-1]+"]");
                                }
                            }
                        }
                    }
                }
            }
        }

        System.out.println("Warmup1 blackhole: " + kernel(100, needles, badNeedles, haystacks));
        // System.out.println("Warmup2 blackhole: " + kernel(100, needles, badNeedles, haystacks));
        // System.out.println("Warmup3 blackhole: " + kernel(100, needles, badNeedles, haystacks));

        // System.out.println("Measure blackhole: " + kernel(100000, needles, badNeedles, haystacks));
        return this;
    }

    int kernel(int iter, String needles[][], String badNeedles[][][], String haystacks[][][][]) {
        int result = 0;
        for (int nIndex = 0; nIndex<needles.length; nIndex++) {
            for (int hIndex = 0 /*nIndex+1*/; hIndex<haystacks[nIndex][0].length; hIndex++) {
                long start = System.nanoTime();
                for (int i = 0; i<iter; i++) {
                    int variant = i%variants;
                    for (int j = 0; j<badNeedles[nIndex][variant].length; j++) {
                        result += haystacks[nIndex][variant][hIndex][j].indexOf2(needles[nIndex][variant], 1);
                        for (int k = 0; k<badNeedles[nIndex][variant].length; k++) {
                            result += haystacks[nIndex][variant][hIndex][j].indexOf2(badNeedles[nIndex][variant][k], 1);
                        }
                    }
                }
                long duration = System.nanoTime() - start;
                System.out.println("Needle: " + needles[nIndex][0].length() + 
                    " Haystack: " + haystacks[nIndex][0][hIndex][0].length() + 
                    " Duration: " + (duration/1000) + " " + hIndex);
            }
        }
        return result;
    }

    enum Encoding {LL, UU, UL; }
    final char a;
    final char aa;
    final char b;
    final char c;
    final char d;
    final Encoding ae;
    HelloString(Encoding _ae) {
        ae = _ae;
        switch (ae) {
            case LL:
                a = 'a';
                aa = a;
                b = 'b';
                c = 'c';
                d = 'd';
                break;
            case UU:
                a = '\u0061';
                aa = a;
                b = '\u0062';
                c = '\u1063';
                d = '\u0064';
                break;
            default: //case UL:
                a = 'a';
                aa = '\u1061';
                b = 'b';
                c = 'c';
                d = 'd';
                break;
        }
    }
    // aaaa+accc(d?)cccb+bbbbbbbbbb
    String newNeedle(int size, int badPosition) {
        return newNeedle(size, badPosition, (char)0);
    }
    String newNeedle(int size, int badPosition, char variant) {
        StringBuilder needle = new StringBuilder(size);
        needle.append((char)(a+variant));
        for (int i=1; i<size-1; i++) {
            if (i == badPosition)
                needle.append((char)(d+variant));
            else
                needle.append((char)(c+variant));
        }
        needle.append((char)(b+variant));
        return needle.toString();
    }

    String newHaystack(int size, String needle, int nPosition) {
        return newHaystack(size, needle, nPosition, (char)0);
    }
    String newHaystack(int size, String needle, int nPosition, char variant) {
        if (nPosition+needle.length()>size) {throw new RuntimeException("Fix testcase "+nPosition+" "+needle.length()+" "+size);}
        StringBuilder haystack = new StringBuilder(size);
        int i = 0;
        for (; i<nPosition; i++) {
            haystack.append((char)(aa+variant));
        }
        haystack.append(needle);
        i += needle.length();
        for (; i<size; i++) {
            haystack.append((char)(b+variant));
        }
        return haystack.toString();
    }
}

// ./build/linux-x86_64-server-fastdebug/images/jdk/bin/java -Xcomp -XX:-TieredCompilation -XX:+UnlockDiagnosticVMOptions HelloString.java
