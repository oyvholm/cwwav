#!/usr/bin/php
<?php

date_default_timezone_set("UTC");
$months = array( 'JAN', 'FEB', 'MAR', 'APR', 'MAJ', 'JUN',
		 'JUL', 'AUG', 'SEP', 'OKT', 'NOV', 'DEC' );

function dkzdate($timestamp)
{
	global $months;
	return date("d", $timestamp) . " " .
		$months[(int)(date("n", $timestamp)) - 1] . " " .
		date("Y", $timestamp) . " " .
		date("Hi", $timestamp) . "z";
}

function stddate($datestr)
{
	// 12. september 2012 14:23
	global $months;
	$match = array();
	$datestr = strtolower($datestr);
	$r = preg_match('|(\d+)\. (\w+) (\d+) (\d+):(\d+)|', $datestr, $match);
	if ($r < 1) {
		echo "Invalid date $datestr\n";
		return "";
	}
	//echo "1: ". $match[1] . " 2: ". $match[2] . "\n";
	$m = 0;
	for ($c=0; $c<12; $c++)
		if (!strncmp(strtolower($months[$c]), $match[2], 3))
			$m = $c+1;
	if ($m == 0) {
		echo "Invalid date $datestr\n";
		return "";
	}
	$s1 = sprintf("%04d-%02d-%02d %02d:%02d", $match[3], $m, $match[1], $match[4], $match[5]);
	//echo "s1: $s1\n";
	$dt = DateTime::createFromFormat('Y-m-j H:i', $s1, new DateTimeZone('Europe/Copenhagen'));
	if (!$dt) {
		echo "DT error:\n";
		var_dump(DateTime::getLastErrors());
		return "";
	}
	$t = $dt->getTimestamp();
	return dkzdate($t);
}


function main()
{
	//echo "Date: ". stddate("24. juni 2009 15:59");
	//exit(0);

	$url = "http://www.dr.dk/nyheder/allenyheder";

	$doc = file_get_contents($url);

	$upddate = dkzdate(time());

	$allnews = "\n\nNyheder fra dr.dk opdateret $upddate =\n\n\n";
	$articles = array();
	//echo $allnews;
	//exit(0);

	$DOM = new DOMDocument;
	libxml_use_internal_errors(true);
	$DOM->loadHTML($doc);
	libxml_use_internal_errors(false);

	// Get the correct section, all "last 24 hours" articles
	// are <h3>'s inside a <section class="dr-list">
	$domxpath = new DOMXPath($DOM);
	$res = $domxpath->query("//section[@class='dr-list']");
	print_r($res);
	if ($res->length != 1) {
		fprintf(STDERR, "Unable to find correct section\n");
		exit(1);
	}
	$section = $res->item(0);
	//print_r($section);
	//exit(0);

	//get all articles
	$items = array();
	$articles = $section->getElementsByTagName('article');
	foreach ($articles as $article) {
		$h = $article->getElementsByTagName('h3');
		if ($h->length != 1) {
			echo "Invalid h3 count ".$h->length."\n";
			continue;
		}
		$h = $h->item(0);
		$headline = trim($h->nodeValue);
		$a = $h->getElementsByTagName('a');
		if ($a->length != 1) {
			echo "Invalid a count ".$h->length."\n";
			continue;
		}
		$a = $a->item(0);
		$storyurl = $a->attributes->getNamedItem('href')->nodeValue;
		if (preg_match('|^/Sporten/|', $storyurl))
			continue;
		echo "u: $storyurl h: " . $headline . "\n";
		continue;
		
		$d = $item->getElementsByTagName('span');
		if (($d->length != 1) || $d->item(0)->getAttribute('class') != "stamp") {
			echo "Invalid date stamp\n";
			continue;
		}
		$datestamp = stddate(trim($d->item(0)->nodeValue));
		//echo "Good story: " . $datestamp . " - " . $headline."\n";

		// Replace <BR>'s with newline
		$brs = $item->getElementsByTagName('br');
		foreach ($brs as $node) {
			$node->parentNode->replaceChild($DOM->createTextNode("\n\n"), $node);
		}
		
		$brs = $item->getElementsByTagName('strong');
		foreach ($brs as $node) {
			$node->parentNode->replaceChild($DOM->createTextNode("= ".trim($node->nodeValue)." =\n\n"), $node);
		}
		
		// Replace paragraphs with double newline
		$content = "";
		$ps = $item->getElementsByTagName('p');
		foreach ($ps as $node) {

			// Replace <STRONG>'s with  "= xxx =\n\n" (subheadings use this)
			$brs = $node->getElementsByTagName('strong');
			foreach ($brs as $node1) {
				$node1->parentNode->replaceChild($DOM->createTextNode("= ".trim($node1->nodeValue)." =\n\n"), $node1);
			}
			
			$p = trim($node->nodeValue);
			if (preg_match('|LÆS OGSÅ:.*|', $p))
				continue;
			$content .= $p . "\n\n";
		}
		
		$content = str_replace(" ", " ", $content); /* &nbsp; */
		$content = str_replace("_", " ", $content);
		$content = preg_replace('|^\s+$|m', '', $content);
		$content = str_replace("\r", "", $content);
		$content = str_replace("\n\n\n", "\n\n", $content);
		$content = str_replace("\n\n\n", "\n\n", $content);
		$content = trim(wordwrap($content, 75));
		//echo "Content:\n" . $content. "\n";
		//break;

		$articles[] = array(
			'datestamp'  => $datestamp,
			'headline'   => $headline,
			'content'    => "*\n\n" . $headline . " =\n\n" . $datestamp . " =\n\n" . $content . "\n\n+\n\n\n");
		
		$allnews .= "*\n\n" . $headline . " =\n\n" . $datestamp . " =\n\n" . $content . "\n\n+\n\n\n";
	}
	
	$allnews .= "\n%\n\n\n";
	file_put_contents("drnews.txt", $allnews);
	$shellscript = "";
        $n = 0;
	foreach ($articles as $a) {
		$n++;
		$fname = sprintf("%02d.txt", $n);
		file_put_contents($fname, $a['content']);
		$shellscript .= "process $fname " . escapeshellarg($a['headline']) . " " . escapeshellarg($a['datestamp']) . "\n";
	}
	file_put_contents("shell-article-list", $shellscript);

	echo "Done - $n articles\n";
}

main();

?>