#include <string>
#include <algorithm>
#include <vector>
#include <stack>
#include <iostream>
#include <stdlib.h>
#include <ctype.h>

#include "ReaderInterface.h"
#include "RtfReader.h"
#include "WriterInterface.h"
#include "UnicodeCharacter.h"
#include "RtfCommand.h"
#include "Style.h"
#include "RtfStyle.h"
#include "debug_global.h"

namespace DoxEngine
{


  unsigned hexToInt(char character)
  {
    if ((character >= '0')&&(character <= '9'))
      return character - '0';
    else if ((character >= 'a')&&(character <= 'z'))
      return character - 'a' + 10;
    else if ((character >= 'A')&&(character <= 'Z'))
      return character - 'A' + 10;
    else return 0;

  }


  RtfReader::RtfReader(std::istream& newStream, WriterInterface& newWriter, DebugLog& newLog):
    log(newLog)
  {
    writer = &newWriter;
    stream = &newStream;
    tableStarted = false;

  }

  RtfReader::~RtfReader()
  {
  }

  bool RtfReader::processData(void)
  {
    char character;
    stream->get(character);

    if (stream->good())
    {

      switch(character)
      {
        case '{':
          // push
          *debugStream << "rtf push" << std::endl;
          rtfStack.push(style);
          break;

        case '}':
          // pop
          *debugStream << "rtf pop" << std::endl;
					style = rtfStack.top();
					if (rtfStack.size())
					{
						rtfStack.pop();
						writer->setStyle(style.getStyle());
					}
					else
					{
							// Mismatched closing brackets
					}
				break;

				case '\\':
					// command
          *debugStream << "command" << std::endl;
          readCommand(character);
          break;


        case '\n':
        case '\r':
        break;

        default:
          // character
          *debugStream << "Writing character" << character << std::endl;
          flushTable();
          if (character >= 32)
          {
            UnicodeCharacter unicodeCharacter(character);
            writer->writeChar(unicodeCharacter);
          }
          break;
      }

      return true;

    }
    else  // End of file
      return false;

  }


  int RtfReader::getPercentComplete(void)
  {
    return 0;
	}



	void RtfReader::readCommand(char inputCharacter)
	{
		std::string inputString;
		char character;

		// need to check for \{ \} \\
		inputString.append(1, inputCharacter);

 	  stream->get(character);
		while (stream->good())
		{

      if (isdigit(character))
      {
        // this is the parameter for the command
        while (isdigit(character) && stream->good())
        {
          inputString.append(1, character);
          stream->get(character);
        }

        if (!isspace(character))
          stream->putback(character);
          
        handleCommand(inputString);
        return;
      }

      
			switch(character)
			{
				case '\'':
				{
					// Numeric representation of a character
					inputString.append(1, character);
					stream->get(character);
					inputString.append(1, character);
					stream->get(character);
					inputString.append(1, character);
					handleCommand(inputString);
					return;
				}


				case '\\':
				{
					stream->putback(character);
					handleCommand(inputString);
					return;
				}


				case '{':
				case '}':
				case ';':
				{
					stream->putback(character);
					handleCommand(inputString);
					return;
				}

				case ' ':
				case '\r':
				case '\n':
				{
					handleCommand(inputString);
					return;
				}

				default:
					inputString.append(1, character);
				break;



      }

      stream->get(character);
    }


  }





  void RtfReader::handleCommand(std::string& inputString)
  {
  using namespace std;

  //cout << "Handling command: " << inputString << "\n";

  if (inputString[1] == '\'')
  {
    unsigned long value =
     (hexToInt(inputString[2])<<4)+hexToInt(inputString[3]);

    UnicodeCharacter character(value);

    commandCharacter(character);

    return;

  }

  string::size_type digitIndex = inputString.find_first_of("-0123456789", 0);
  string commandString;

  if (digitIndex>0)
    commandString = inputString.substr(1, digitIndex-1);
  else
    commandString = inputString.substr(1);

  //cout << "digitIndex = " << digitIndex << "\n";

  int value;
  if (digitIndex >= inputString.length())
    value = 1;
  else
  {
    string valueString(inputString, digitIndex);
    value = atoi(valueString.c_str());
    //value = 1;
  }


  RtfCommands::iterator found = elements.find(commandString);

  if (found != elements.end())
    found->second->handleCommand(this, value);
  //else
  //  cout << "Command " << commandString << " not found\n";


  }


  void RtfReader::commandIgnoreDestinationKeyword(void)
  {
    int level;
		char character;


		for (level=1;level!=0 && stream->good();)
		{
      stream->get(character);

      if (character == '{')
        level++;
      else if (character == '}')
        level--;

    }


  }






  void RtfReader::commandParagraphBreak(void)
  {
    writer->writeBreak(ParagraphBreak);
  }

  void RtfReader::commandLineBreak(void)
  {
    writer->writeBreak(LineBreak);
  }



  void RtfReader::commandCharacter(UnicodeCharacter& character)
  {
    writer->writeChar(character);
  }


  void RtfReader::commandParagraphDefault(void)
  {
		style.setInTable( false );
		Style standardStyle = style.getStyle();
		standardStyle.setJustification( DefaultJustified );
		style.setStyle(standardStyle);
    writer->setStyle( standardStyle );
  }

  void RtfReader::commandInTable(void)
  {
    // Note: RTF does not allow nested tables
		style.setInTable( true );

	}

	void RtfReader::flushTable(void)
	{
		if (style.getInTable() || style.getSectionColumns())
		{
  		if (!tableStarted)
	  	{
		  	// Beginning of table
			  writer->writeTable(TableStart);
  			tableStarted = true;

	  		writer->writeTable(TableRowStart);
		  	rowStarted = true;

			  writer->writeTable(TableCellStart);
  			cellStarted = true;
	  	}


			if (!rowStarted)
			{
				writer->writeTable(TableRowStart);
				rowStarted = true;
			}

			if (!cellStarted)
			{
				writer->writeTable(TableCellStart);
        cellStarted = true;
      }


    }
    else if (tableStarted)
    {
      writer->writeTable(TableEnd);
      tableStarted = false;
    }
  }

  void RtfReader::commandEndCell(void)
  {
    if (cellStarted)
      writer->writeTable(TableCellEnd);

    cellStarted = false;
	}

  void RtfReader::commandEndRow(void)
  {
    commandEndCell();

    if (rowStarted)
      writer->writeTable(TableRowEnd);

    rowStarted = false;
  }


	Style RtfReader::getStyle(void) const
	{
		return style.getStyle();
	}

	void RtfReader::setStyle( const Style &value )
	{
		//cout << "Setting style\n";
		style.setStyle(value);
		writer->setStyle( value );
	}

	void RtfReader::commandColourTable(void)
	{
		int brackets = 1;
		char character;
		std::string inputString;

    stream->get(character);

		while (brackets && stream->good())
		{
			//cout << "Colour character = " << character;

			if (inputString.length() > 0)
			{
				switch(character)
        {
          case '\\':
          case '{':
          case '}':
          case ';':
          {
            stream->putback(character);
            // handle the colour commands
            std::string::size_type digitIndex = inputString.find_first_of("-0123456789", 0);
            std::string commandString(inputString, 1, digitIndex-1);

            //cout << "inputString.length() = " << inputString.length() << "\n";
            //cout << "digitIndex = " << digitIndex << "\n";
						//cout << "inputString = " << inputString << "\n";

						int value;

            if (digitIndex >= inputString.length())
              value = 1;
            else
            {
              std::string valueString(inputString, digitIndex);
              value = atoi(valueString.c_str());
              //cout << "value = " << value << "\n";
            }

            //cout << "commandString = \"" << commandString << "\"\n";
            //cout << "commandString.length() = " << commandString.length() << "\n";

						if (commandString == "red")
						{
							//cout << "red\n";
							style.setRed(value);
            }
            else if (commandString == "green")
            {
              //cout << "green\n";
							style.setGreen(value);
            }
            else if (commandString == "blue")
            {
              //cout << "blue\n";
              style.setBlue(value);
            }
            //else
						//  cout << "Not recognised\n";

						inputString.assign("");
          }
          break;

          default:
            //cout << "character = " << character << "\n";
            inputString.append(1, (char)character);
            //cout << "inputString = " << inputString << "\n";
          break;
        }
      }
      else
      {
        //cout << "Contol character: " << character;
				switch(character)
				{
					case '{':
            brackets++;
          break;

          case '}':
            brackets--;
          break;

          case ';':
            style.colourTerminate();
          break;

          case '\\':
            inputString.append(1, (char)character);
					break;
				}
			}

      if (brackets)
        stream->get(character);
		}

	}

	RtfStyle RtfReader::getRtfStyle(void) const
	{
		return style;
	}

	void RtfReader::setRtfStyle( const RtfStyle &value )
	{
		style = value;
		writer->setStyle( value.getStyle() );
	}


}



