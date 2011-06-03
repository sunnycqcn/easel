/* I/O of multiple sequence alignments in PSI-BLAST format
 * 
 * Contents:
 *   1. API for reading/writing PSI-BLAST format
 *   2. Example.
 *   3. Copyright and license information.
 */
#include "esl_config.h"

#include <stdio.h>
#include <ctype.h>

#include "easel.h"
#ifdef eslAUGMENT_ALPHABET
#include "esl_alphabet.h"
#endif
#include "esl_mem.h"
#include "esl_msa.h"
#include "esl_msafile.h"
#include "esl_msafile_psiblast.h"

/*****************************************************************
 *# 1. API for reading/writing PSI-BLAST format
 *****************************************************************/

/* Function:  esl_msafile_psiblast_SetInmap()
 * Synopsis:  Set input map specific for PSI-BLAST input.
 *
 * Purpose:   The main msafile interface has set a default inmap
 *            and is asking us to make it specific for PSI-BLAST format.
 *            
 *            PSI-BLAST only allows - for a gap. It also disallows O residues.
 */
int
esl_msafile_psiblast_SetInmap(ESLX_MSAFILE *afp)
{
  afp->inmap['.'] = eslDSQ_ILLEGAL;
  afp->inmap['_'] = eslDSQ_ILLEGAL;
  afp->inmap['*'] = eslDSQ_ILLEGAL;
  afp->inmap['~'] = eslDSQ_ILLEGAL;
  afp->inmap['O'] = eslDSQ_ILLEGAL;
  afp->inmap['o'] = eslDSQ_ILLEGAL;
  return eslOK;
}

/* Function:  esl_msafile_psiblast_GuessAlphabet()
 * Synopsis:  Guess the alphabet of an open PSI-BLAST MSA file.
 *
 * Purpose:   Guess the alpbabet of the sequences in open
 *            PSI-BLAST format MSA file <afp>.
 *            
 *            On a normal return, <*ret_type> is set to <eslDNA>,
 *            <eslRNA>, or <eslAMINO>, and <afp> is reset to its
 *            original position.
 *
 * Args:      afp      - open PSI-BLAST format MSA file
 *            ret_type - RETURN: <eslDNA>, <eslRNA>, or <eslAMINO>       
 *
 * Returns:   <eslOK> on success.
 *            <eslENOALPHABET> if alphabet type can't be determined.
 *            In either case, <afp> is rewound to the position it
 *            started at.
 */
int
esl_msafile_psiblast_GuessAlphabet(ESLX_MSAFILE *afp, int *ret_type)
{
  int       alphatype     = eslUNKNOWN;
  esl_pos_t anchor        = -1;
  int       threshold[3]  = { 500, 5000, 50000 }; /* we check after 500, 5000, 50000 residues; else we go to EOF */
  int       nsteps        = 3;
  int       step          = 0;
  int       nres          = 0;
  int       x;
  int64_t   ct[26];
  char     *p, *tok;
  esl_pos_t n,  toklen, pos;
  int       status;

  for (x = 0; x < 26; x++) ct[x] = 0;

  anchor = esl_buffer_GetOffset(afp->bf);
  if ((status = esl_buffer_SetAnchor(afp->bf, anchor)) != eslOK) { status = eslEINCONCEIVABLE; goto ERROR; } /* [eslINVAL] can't happen here */

  while ( (status = eslx_msafile_GetLine(afp, &p, &n)) == eslOK)
    {
      if ((status = esl_memtok(&p, &n, " \t", &tok, &toklen)) != eslOK) continue; /* blank lines */
      /* p now points to the rest of the sequence line, after a name */
      
      /* count characters into ct[] array */
      for (pos = 0; pos < n; pos++)
	if (isalpha(p[pos])) {
	  x = toupper(p[pos]) - 'A';
	  ct[x]++;
	  nres++; 
	}

      /* try to stop early, checking after 500, 5000, and 50000 residues: */
      if (step < nsteps && nres > threshold[step]) {
	if ((status = esl_abc_GuessAlphabet(ct, &alphatype)) == eslOK) goto DONE; /* (eslENOALPHABET) */
	step++;
      }
    }
  if (status != eslEOF) goto ERROR; /* [eslEMEM,eslESYS,eslEINCONCEIVABLE] */
  status = esl_abc_GuessAlphabet(ct, &alphatype); /* (eslENOALPHABET) */

 DONE:
  esl_buffer_SetOffset(afp->bf, anchor);   /* Rewind to where we were. */
  esl_buffer_RaiseAnchor(afp->bf, anchor);
  *ret_type = alphatype;
  return status;

 ERROR:
  if (anchor != -1) {
    esl_buffer_SetOffset(afp->bf, anchor);
    esl_buffer_RaiseAnchor(afp->bf, anchor);
  }
  *ret_type = eslUNKNOWN;
  return status;
}

/* Function:  esl_msafile_psiblast_Read()
 * Synopsis:  Read an alignment in PSI-BLAST's input format.
 *
 * Purpose:   Read an MSA from an open <ESLX_MSAFILE> <afp>, parsing for
 *            PSI-BLAST input format, starting from the current point.
 *            Create a new multiple alignment, and return a ptr to 
 *            that alignment via <*ret_msa>. Caller is responsible for
 *            free'ing this <ESL_MSA>.
 *            
 *            The <msa> has a reference line (<msa->rf[]>) that
 *            corresponds to the uppercase/lowercase columns in the
 *            alignment: consensus (uppercase) columns are marked 'X',
 *            and insert (lowercase) columns are marked '.' in this RF
 *            line.
 *            
 * Args:      afp     - open <ESL_MSAFILE>
 *            ret_msa - RETURN: newly parsed <ESL_MSA>
 *
 * Returns:   <eslOK> on success. <*ret_msa> contains the newly
 *            allocated MSA. <afp> is at EOF.
 *
 *            <eslEOF> if no (more) alignment data are found in
 *            <afp>, and <afp> is returned at EOF. 
 *
 *            <eslEFORMAT> on a parse error. <*ret_msa> is set to
 *            <NULL>. <afp> contains information sufficient for
 *            constructing useful diagnostic output: 
 *            | <afp->errmsg>       | user-directed error message     |
 *            | <afp->linenumber>   | line # where error was detected |
 *            | <afp->line>         | offending line (not NUL-term)   |
 *            | <afp->n>            | length of offending line        |
 *            | <afp->bf->filename> | name of the file                |
 *            and <afp> is poised at the start of the following line,
 *            so (in principle) the caller could try to resume
 *            parsing.
 *
 * Throws:    <eslEMEM> on allocation error.
 *            <eslESYS> if a system call fails, such as fread().
 *            <eslEINCONCEIVABLE> - "impossible" corruption 
 *            On these, <*ret_msa> is returned <NULL>, and the state of
 *            <afp> is undefined.
 */
int
esl_msafile_psiblast_Read(ESLX_MSAFILE *afp, ESL_MSA **ret_msa)
{
  ESL_MSA  *msa      = NULL;
  int       idx      = 0;	/* counter over sequences in a block */
  int       nblocks  = 0;	/* counter over blocks */
  int64_t   alen     = 0;	
  int       nseq     = 0;
  int64_t   cur_alen;
  esl_pos_t pos;		/* position on a line */
  esl_pos_t name_start,      name_len;
  esl_pos_t seq_start,       seq_len;
  esl_pos_t block_seq_start, block_seq_len;
  int       status;

  afp->errmsg[0] = '\0';
  
  /* allocate a growable MSA. We set msa->{nseq,alen} only when we're done. */
#ifdef eslAUGMENT_ALPHABET
  if (afp->abc   &&  (msa = esl_msa_CreateDigital(afp->abc, 16, -1)) == NULL) { status = eslEMEM; goto ERROR; }
#endif
  if (! afp->abc &&  (msa = esl_msa_Create(                 16, -1)) == NULL) { status = eslEMEM; goto ERROR; }

  /* skip leading blank lines in file */
  while ( (status = eslx_msafile_GetLine(afp, NULL, NULL)) == eslOK && esl_memspn(afp->line, afp->n, " \t") == afp->n) ;
  if (status != eslOK)  goto ERROR; /* includes normal EOF */
  
  /* Read the file a line at a time; if a parsing error occurs, detect immediately, with afp->linenumber set correctly */
   do { /* while in the file... */
    idx = 0;
    do { /* while in a block... */
      for (pos = 0;     pos < afp->n; pos++) if (! isspace(afp->line[pos])) break;  name_start = pos; 
      for (pos = pos+1; pos < afp->n; pos++) if (  isspace(afp->line[pos])) break;  name_len   = pos - name_start;
      for (pos = pos+1; pos < afp->n; pos++) if (! isspace(afp->line[pos])) break;  seq_start  = pos;      
      if (pos >= afp->n) ESL_XFAIL(eslEFORMAT, afp->errmsg, "invalid alignment line");
      for (pos = afp->n-1; pos > 0; pos--)   if (! isspace(afp->line[pos])) break;  seq_len    = pos - seq_start + 1;

      if (idx == 0) {
	block_seq_start = seq_start;
	block_seq_len   = seq_len;
      } else {
	if (seq_start != block_seq_start) ESL_XFAIL(eslEFORMAT, afp->errmsg, "sequence start is misaligned");
	if (seq_len   != block_seq_len)   ESL_XFAIL(eslEFORMAT, afp->errmsg, "sequence end is misaligned");
      }
      
      /* Process the consensus #=RF line. */
      if (idx == 0) {
	ESL_REALLOC(msa->rf, sizeof(char) * (alen + seq_len + 1));
	for (pos = 0; pos < seq_len; pos++) msa->rf[alen+pos] = '-'; /* anything neutral other than . or X will do. */
	msa->rf[alen+pos] = '\0';
      }
      for (pos = 0; pos < seq_len; pos++) 
	{
	  if (afp->line[seq_start+pos] == '-') continue;
	  if (isupper(afp->line[seq_start+pos])) {
	    if (msa->rf[alen+pos] == '.') ESL_XFAIL(eslEFORMAT, afp->errmsg, "unexpected upper case residue (#%d on line)", (int) pos+1);
	    msa->rf[alen+pos] = 'X';
	  }
	  if (islower(afp->line[seq_start+pos])) {
	    if (msa->rf[alen+pos] == 'X') ESL_XFAIL(eslEFORMAT, afp->errmsg, "unexpected lower case residue (#%d on line)", (int) pos+1);
	    msa->rf[alen+pos] = '.';
	  }
	}

      /* Store the sequence name. */
      if (nblocks == 0)	{
	/* make sure we have room for another sequence */
	if (idx >= msa->sqalloc &&  (status = esl_msa_Expand(msa))                   != eslOK) goto ERROR;
	if ( (status = esl_msa_SetSeqName(msa, idx, afp->line+name_start, name_len)) != eslOK) goto ERROR;
      } else {
	if (! esl_memstrcmp(afp->line+name_start, name_len, msa->sqname[idx]))
	  ESL_XFAIL(eslEFORMAT, afp->errmsg, "expected sequence %s on this line, but saw %.*s", msa->sqname[idx], (int) name_len, afp->line+name_start);
      }

      /* Append the sequence. */
      cur_alen = alen;
#ifdef eslAUGMENT_ALPHABET
      if (msa->abc)    { status = esl_abc_dsqcat(afp->inmap, &(msa->ax[idx]),   &(cur_alen), afp->line+seq_start, seq_len); }
#endif
      if (! msa->abc)  { status = esl_strmapcat (afp->inmap, &(msa->aseq[idx]), &(cur_alen), afp->line+seq_start, seq_len); }
      if      (status == eslEINVAL)    ESL_XFAIL(eslEFORMAT, afp->errmsg, "one or more invalid sequence characters");
      else if (status != eslOK)        goto ERROR;
      if (cur_alen - alen != seq_len)  ESL_XFAIL(eslEFORMAT, afp->errmsg, "unexpected number of seq characters");
      
      /* get next line. if it's blank, or if we're EOF, we're done with the block */
      idx++;
      status = eslx_msafile_GetLine(afp, NULL, NULL);
    } while (status == eslOK && esl_memspn(afp->line, afp->n, " \t") < afp->n); /* blank line ends a block. */
    if (status != eslOK && status != eslEOF) goto ERROR; 
    /* End of one block */
    
    if     (nblocks == 0) nseq = idx;
    else if (idx != nseq) ESL_XFAIL(eslEFORMAT, afp->errmsg, "last block didn't contain same # of seqs as earlier blocks");
    alen += block_seq_len;
    nblocks++;

    /* skip blank lines to start of next block, if any */
    while ( (status = eslx_msafile_GetLine(afp, NULL, NULL)) == eslOK  && esl_memspn(afp->line, afp->n, " \t") == afp->n) ;
   } while (status == eslOK);
   if (status != eslEOF) goto ERROR;
   
   msa->nseq = nseq;
   msa->alen = alen;
   *ret_msa  = msa;
   return eslOK;

 ERROR:
  if (msa) esl_msa_Destroy(msa);
  *ret_msa = NULL;
  return status;
}


/* Function:  esl_msafile_psiblast_Write()
 * Synopsis:  Write an MSA to a stream in PSI-BLAST format
 *
 * Purpose:   Write alignment <msa> in NCBI PSI-BLAST format to 
 *            stream <fp>.
 *            
 *            The <msa> should have a valid reference line <msa->rf>,
 *            with alphanumeric characters marking consensus (match)
 *            columns, and non-alphanumeric characters marking
 *            nonconsensus (insert) columns. If it does not have RF
 *            annotation, then the first sequence in the <msa> 
 *            defines the "consensus".
 *            
 *            PSI-BLAST format allows only one symbol ('-') for gaps,
 *            and cannot represent missing data symbols (Easel's
 *            '~'). Any missing data symbols are converted to gaps.
 *
 * Args:      fp  - open output stream
 *            msa - MSA to write       
 *
 * Returns:   <eslOK> on success.
 *
 * Throws:    <eslEMEM> on allocation failure.
 */
int
esl_msafile_psiblast_Write(FILE *fp, const ESL_MSA *msa)
{
  char    *buf = NULL;
  int      cpl = 60;
  int      acpl;
  int      i;
  int      sym;
  int64_t  pos, bpos;
  int      maxnamewidth = esl_str_GetMaxWidth(msa->sqname, msa->nseq);
  int      is_consensus;
  int      is_residue;
  int      status;

  ESL_ALLOC(buf, sizeof(char) * (cpl+1));

  for (pos = 0; pos < msa->alen; pos += cpl)
    {
      for (i = 0; i < msa->nseq; i++)
	{
	  acpl =  (msa->alen - pos > cpl)? cpl : msa->alen - pos;

#ifdef eslAUGMENT_ALPHABET
	  if (msa->abc)
	    {
	      for (bpos = 0; bpos < acpl; bpos++)
		{
		  sym          = msa->abc->sym[msa->ax[i][pos + bpos + 1]];
		  is_residue   = esl_abc_XIsResidue(msa->abc, msa->ax[i][pos+bpos+1]);
		  if (msa->rf) is_consensus = (isalnum(msa->rf[pos + bpos]) ? TRUE : FALSE);
		  else         is_consensus = (esl_abc_XIsResidue(msa->abc, msa->ax[0][pos+bpos+1]) ? TRUE : FALSE);
				      
		  if (is_consensus) { buf[bpos] = (is_residue ? toupper(sym) : '-'); }
		  else              { buf[bpos] = (is_residue ? tolower(sym) : '-'); }
		}
	    }
#endif
	  if (! msa->abc)
	    {
	      for (bpos = 0; bpos < acpl; bpos++)
		{
		  sym          = msa->aseq[i][pos + bpos];
		  is_residue   = isalnum(sym);
		  if (msa->rf) is_consensus = (isalnum(msa->rf[pos + bpos]) ? TRUE : FALSE);
		  else         is_consensus = (isalnum(msa->aseq[0][pos+bpos]) ? TRUE : FALSE);

		  if (is_consensus) { buf[bpos] = (is_residue ? toupper(sym) : '-'); }
		  else              { buf[bpos] = (is_residue ? tolower(sym) : '-'); }
		}
	    }
	  buf[acpl] = '\0';	      
	  fprintf(fp, "%-*s  %s\n", maxnamewidth, msa->sqname[i], buf);
	}  /* end loop over sequences */

      if (pos + cpl < msa->alen) fputc('\n', fp);
    }
  free(buf);
  return eslOK;

 ERROR:
  if (buf) free(buf);
  return status;
}
/*----------- end, API for i/o of psi-blast format --------------*/



/*****************************************************************
 * 2. Example.
 *****************************************************************/

#ifdef eslMSAFILE_PSIBLAST_EXAMPLE
/* An example of reading an MSA in text mode, and handling any returned errors.
   gcc -g -Wall -o esl_msafile_psiblast_example -I. -DeslMSAFILE_PSIBLAST_EXAMPLE esl_msafile_psiblast.c esl_msa.c easel.c 
   ./esl_msafile_psiblast_example <msafile>
 */

/*::cexcerpt::msafile_psiblast_example::begin::*/
#include <stdio.h>

#include "easel.h"
#include "esl_msa.h"
#include "esl_msafile.h"
#include "esl_msafile_psiblast.h"

int 
main(int argc, char **argv)
{
  char         *filename = argv[1];
  int           fmt      = eslMSAFILE_PSIBLAST;
  ESLX_MSAFILE *afp      = NULL;
  ESL_MSA      *msa      = NULL;
  int           status;

  if ( (status = eslx_msafile_Open(NULL, filename, fmt, NULL, &afp)) != eslOK) 
    eslx_msafile_OpenFailure(afp, status);

  if ( (status = esl_msafile_psiblast_Read(afp, &msa))         != eslOK)
    eslx_msafile_ReadFailure(afp, status);

  printf("alignment %5d: %15s: %6d seqs, %5d columns\n", 
	 1, msa->name, msa->nseq, (int) msa->alen);

  esl_msafile_psiblast_Write(stdout, msa);
  esl_msa_Destroy(msa);
  eslx_msafile_Close(afp);
  exit(0);
}
/*::cexcerpt::msafile_psiblast_example::end::*/
#endif /*eslMSAFILE_PSIBLAST_EXAMPLE*/
/*--------------------- end of example --------------------------*/



/*****************************************************************
 * @LICENSE@
 *
 * SVN $Id$
 * SVN $URL$
 *****************************************************************/
