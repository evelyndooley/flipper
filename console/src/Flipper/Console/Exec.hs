module Flipper.Console.Exec where

import Control.Monad.Trans.Class

import qualified Data.ByteString as BS

import Flipper.MonadFlipper

import qualified Flipper.Button as Button
import qualified Flipper.FS     as FS
import qualified Flipper.GPIO   as GPIO
import qualified Flipper.LED    as LED
import qualified Flipper.CPU    as CPU
import qualified Flipper.SPI    as SPI
import qualified Flipper.UART   as UART

import Flipper.Console.Action
import Flipper.Console.FC
import Flipper.Console.Flash
import Flipper.Console.Parsers

import System.Console.Haskeline

import qualified Text.Megaparsec        as M

execConsoleAction :: ConsoleAction -> FC ()
execConsoleAction (Flash f)       = execFlash f
execConsoleAction (Install m f)   = execInstall m f
execConsoleAction (Launch s)      = execLaunch s
execConsoleAction Reset           = execReset
execConsoleAction Suspend         = execSuspend
execConsoleAction Format          = execFormat
execConsoleAction (ConsoleCall c) = execCall c

execInstall :: ModuleID -> FilePath -> FC ()
execInstall = undefined

execLaunch :: String -> FC ()
execLaunch = undefined

execReset :: FC ()
execReset = CPU.reset

execSuspend :: FC ()
execSuspend = CPU.halt

execFormat :: FC ()
execFormat = FS.format

execCall :: Call -> FC ()
execCall (ButtonCall b) = execButtonAction b
execCall (FSCall f)     = execFSAction f
execCall (GPIOCall g)   = execGPIOAction g
execCall (LEDCall l)    = execLEDAction l
execCall (SPICall s)    = execSPIAction s
execCall (UARTCall u)   = execUARTAction u

execButtonAction :: ButtonAction -> FC ()
execButtonAction ButtonRead = Button.read >>= printFC

execFSAction :: FSAction -> FC ()
execFSAction (FSCreateFromString n c) = FS.create n c
execFSAction (FSCreateFromFile n fp)  = liftFC (BS.readFile fp) >>= FS.create n
execFSAction (FSRemove n)             = FS.remove n
execFSAction (FSRename t f)           = FS.rename t f

execGPIOAction :: GPIOAction -> FC ()
execGPIOAction (GPIODigitalDirection p d) = GPIO.digitalDirection p d
execGPIOAction (GPIODigitalRead p)        = GPIO.digitalRead p >>= printFC
execGPIOAction (GPIODigitalWrite p v)     = GPIO.digitalWrite p v
execGPIOAction (GPIOAnalogDirection p d)  = GPIO.analogDirection p d
execGPIOAction (GPIOAnalogRead p)         = GPIO.analogRead p >>= printFC
execGPIOAction (GPIOAnalogWrite p v)      = GPIO.analogWrite p v

execLEDAction :: LEDAction -> FC ()
execLEDAction (LEDSetRGB c) = LED.setRGB c

execSPIAction :: SPIAction -> FC ()
execSPIAction SPIEnable              = SPI.enable
execSPIAction SPIDisable             = SPI.disable
execSPIAction SPIRead                = SPI.pull >>= printCStringFC
execSPIAction (SPIWriteFromString s) = SPI.push s
execSPIAction (SPIWriteFromFile fp)  = liftFC (BS.readFile fp) >>= SPI.push

execUARTAction :: UARTAction -> FC ()
execUARTAction UARTEnable              = UART.enable
execUARTAction UARTDisable             = UART.disable
execUARTAction UARTRead                = UART.pull >>= printCStringFC
execUARTAction (UARTWriteFromString s) = UART.push s
execUARTAction (UARTWriteFromFile fp)  = liftFC (BS.readFile fp) >>= UART.push

fcREPL :: FC ()
fcREPL = do
    i <- lift $ getInputLine "flipper>"
    case i of Nothing  -> return ()
              (Just l) -> execUserInput l *> fcREPL

execUserInput :: String -> FC ()
execUserInput l = case M.runParser parseConsoleAction "<interactive>" l of
    (Left e)  -> lift (outputStrLn (show e))
    (Right c) -> catchFlipper (execConsoleAction c)
                              (reportConsoleError (Just c))
