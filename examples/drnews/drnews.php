#!/usr/bin/php
<?php

   $url = "http://www.dr.dk/nyheder/allenyheder";
   $doc = file_get_contents($url);

   $allnews = "\n\nNyheder fra dr.dk\n\n\n";

   $DOM = new DOMDocument;
   $DOM->loadHTML($doc);

   //get all H1
   $items = $DOM->getElementsByTagName('div');

   //echo "Encoding: ".$DOM->encoding."\n";

// display all H1 text
foreach ($items as $item) {
	//echo $item->getAttribute('id') . "\n";
	if (!strcasecmp($item->getAttribute('class'), "txtContent")) {
		//echo "Content node found\n";
		$h = ($item->getElementsByTagName('h1'));
		if ($h->length != 1) {
			echo "Invalid length ".$h->length."\n";
			continue;
		}
		$headline = trim($h->item(0)->nodeValue);
		$d = $item->getElementsByTagName('span');
		if (($d->length != 1) || $d->item(0)->getAttribute('class') != "stamp") {
			echo "Invalid date stamp\n";
			continue;
		}
		$datestamp = trim($d->item(0)->nodeValue);
		echo "Good story: " . $datestamp . " - " . $headline."\n";

		// Replace <BR>'s with newline
		$brs = $item->getElementsByTagName('br');
		foreach ($brs as $node) {
			$node->parentNode->replaceChild($DOM->createTextNode("\n\n"), $node);
		}

		// Replace <STRONG>'s with  "= xxx =\n\n" (subheadings use this)
		$brs = $item->getElementsByTagName('strong');
		foreach ($brs as $node) {
			$node->parentNode->replaceChild($DOM->createTextNode("= ".trim($node->nodeValue)." =\n"), $node);
		}
		
		// Replace paragraphs with double newline
		$content = "";
		$ps = $item->getElementsByTagName('p');
		foreach ($ps as $node) {
			$content .= trim($node->nodeValue) ."\n\n";
		}

		$content = str_replace("\r", "", $content);
		$content = str_replace("\n\n\n", "\n\n", $content);
		$content = str_replace("\n\n\n", "\n\n", $content);
		$content = trim(wordwrap($content, 75));
		echo "Content:\n" . $content. "\n";
		//break;

		$allnews .= "* " . $headline . "\n\n" . $datestamp . " =\n\n" . $content . "\n+\n\n";
	}

}
$allnews .= "\n%\n\n\n";
file_put_contents("drnews.txt", $allnews);

echo "Done\n";

?>